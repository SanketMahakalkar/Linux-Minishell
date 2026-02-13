
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64
#define MAX_JOBS 20

// --- Global State ---
int last_exit_status = 0;
pid_t shell_pid;
char shell_path[PATH_MAX];
pid_t foreground_pid = -1; // PID of the currently running fg process

// Job Control Structure
typedef struct {
    pid_t pid;
    char command[MAX_CMD_LEN];
    int status; // 0: Running, 1: Stopped
} Job;

Job jobs[MAX_JOBS];
int job_count = 0;

// --- Helper Functions ---

// Add a job to the tracking list
void add_job(pid_t pid, char *cmd, int status) {
    if (job_count < MAX_JOBS) {
        jobs[job_count].pid = pid;
        strncpy(jobs[job_count].command, cmd, MAX_CMD_LEN);
        jobs[job_count].status = status;
        job_count++;
        if (status == 0) printf("[%d] %d\n", job_count, pid);
    } else {
        fprintf(stderr, "Job list full!\n");
    }
}

// Remove a job (usually after it finishes)
void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            // Shift remaining jobs left
            for (int j = i; j < job_count - 1; j++) {
                jobs[j] = jobs[j + 1];
            }
            job_count--;
            break;
        }
    }
}

// Find job index by PID
int find_job_by_pid(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) return i;
    }
    return -1;
}

// --- Signal Handlers ---

void handle_sigint(int sig) {
    printf("\n");
    // If a foreground process is running, the signal is sent to the process group,
    // so the child receives it automatically. We just need to ensure the shell prints a new line.
    if (foreground_pid == -1) {
        // If no foreground process, reprint prompt
        char *ps1 = getenv("PS1");
        if (!ps1) ps1 = "msh> ";
        printf("%s", ps1);
        fflush(stdout);
    }
}

void handle_sigtstp(int sig) {
    // Ctrl+Z logic handled in the waitpid logic of the parent
    // Just create a newline here for visual cleanness
    printf("\n");
}

void handle_sigchld(int sig) {
    int status;
    pid_t pid;

    // Reap all dead children (zombies), strictly non-blocking
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        if (pid == foreground_pid) {
            // Foreground process handled in the main execution loop, ignore here
            continue;
        }

        // Handle Background processes
        int job_idx = find_job_by_pid(pid);
        if (job_idx != -1) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                printf("\n[%d]+  Done\t\t%s\n", job_idx + 1, jobs[job_idx].command);
                remove_job(pid);
                
                // Re-print prompt if interactive
                char *ps1 = getenv("PS1");
                if (!ps1) ps1 = "msh> ";
                printf("%s", ps1);
                fflush(stdout);
            }
        }
    }
}

// --- Built-in Commands ---

void exec_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "msh: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("msh");
            last_exit_status = 1;
        } else {
            last_exit_status = 0;
        }
    }
}

void exec_pwd() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
        last_exit_status = 0;
    } else {
        perror("pwd");
        last_exit_status = 1;
    }
}

void exec_jobs() {
    for (int i = 0; i < job_count; i++) {
        printf("[%d] %s %s [%d]\n", i + 1, 
               (jobs[i].status == 0) ? "Running" : "Stopped", 
               jobs[i].command, jobs[i].pid);
    }
    last_exit_status = 0;
}

void exec_fg(char **args) {
    if (job_count == 0) {
        printf("msh: no current jobs\n");
        return;
    }
    
    // Default to last job if no PID given
    int job_idx = job_count - 1; 
    if (args[1] != NULL) {
        pid_t target_pid = atoi(args[1]);
        job_idx = find_job_by_pid(target_pid);
    }

    if (job_idx == -1) {
        printf("msh: job not found\n");
        return;
    }

    pid_t pid = jobs[job_idx].pid;
    char cmd_copy[MAX_CMD_LEN];
    strcpy(cmd_copy, jobs[job_idx].command);

    // Remove from jobs list temporarily as it moves to foreground
    remove_job(pid);

    foreground_pid = pid;
    printf("%s\n", cmd_copy);
    
    // Send SIGCONT in case it was stopped
    kill(pid, SIGCONT);

    // Wait for it
    int status;
    waitpid(pid, &status, WUNTRACED);
    foreground_pid = -1;

    if (WIFSTOPPED(status)) {
        add_job(pid, cmd_copy, 1); // 1 = Stopped
    } else {
        last_exit_status = WEXITSTATUS(status);
    }
}

void exec_bg(char **args) {
     if (job_count == 0) {
        printf("msh: no current jobs\n");
        return;
    }
    
    // Default to last job
    int job_idx = job_count - 1;
    if (args[1] != NULL) {
        pid_t target_pid = atoi(args[1]);
        job_idx = find_job_by_pid(target_pid);
    }

    if (job_idx == -1 || jobs[job_idx].status == 0) {
        printf("msh: job not found or already running\n");
        return;
    }

    pid_t pid = jobs[job_idx].pid;
    jobs[job_idx].status = 0; // Set to Running
    printf("[%d]+ %s &\n", job_idx + 1, jobs[job_idx].command);
    kill(pid, SIGCONT);
}

// --- Variable Parsing ---

void expand_variables(char *cmd) {
    char buffer[MAX_CMD_LEN] = {0};
    char *src = cmd;
    char *dst = buffer;
    
    while (*src) {
        if (*src == '$') {
            src++;
            if (*src == '?') {
                char temp[16];
                sprintf(temp, "%d", last_exit_status);
                strcat(dst, temp);
                dst += strlen(temp);
                src++;
            } else if (*src == '$') {
                char temp[16];
                sprintf(temp, "%d", shell_pid);
                strcat(dst, temp);
                dst += strlen(temp);
                src++;
            } else if (strncmp(src, "SHELL", 5) == 0) {
                strcat(dst, shell_path);
                dst += strlen(shell_path);
                src += 5;
            } else {
                *dst++ = '$'; // just a normal $
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    strcpy(cmd, buffer);
}

// --- Command Execution Logic ---

void execute_pipeline(char *line) {
    // Handle Variable Assignment (PS1=NEW)
    if (strchr(line, '=') && strncmp(line, "PS1", 3) == 0) {
        char *eq = strchr(line, '=');
        if (eq && *(eq-1) != ' ' && *(eq+1) != ' ') {
            *eq = '\0';
            setenv(line, eq + 1, 1);
            return;
        }
    }

    int is_bg = 0;
    // Check for background '&'
    int len = strlen(line);
    if (line[len-1] == '&') {
        is_bg = 1;
        line[len-1] = '\0'; // Remove &
    }

    // Split by pipe
    char *commands[MAX_ARGS];
    int n_cmds = 0;
    char *token = strtok(line, "|");
    while (token != NULL && n_cmds < MAX_ARGS) {
        commands[n_cmds++] = token;
        token = strtok(NULL, "|");
    }

    int pipefd[2 * (n_cmds - 1)]; // 2 fds per pipe
    for (int i = 0; i < n_cmds - 1; i++) {
        if (pipe(pipefd + i * 2) < 0) {
            perror("pipe");
            return;
        }
    }

    pid_t pids[MAX_ARGS];
    int cmd_idx = 0;

    for (int i = 0; i < n_cmds; i++) {
        // Tokenize args for this command segment
        char *arg_tokens[MAX_ARGS];
        int arg_count = 0;
        char *subtoken = strtok(commands[i], " \t\n");
        while (subtoken != NULL) {
            arg_tokens[arg_count++] = subtoken;
            subtoken = strtok(NULL, " \t\n");
        }
        arg_tokens[arg_count] = NULL;

        if (arg_count == 0) continue;

        // Built-ins (Only run in parent if single command, no pipe)
        if (n_cmds == 1 && strcmp(arg_tokens[0], "exit") == 0) {
            exit(0);
        }
        if (n_cmds == 1 && strcmp(arg_tokens[0], "cd") == 0) {
            exec_cd(arg_tokens);
            return;
        }
        if (n_cmds == 1 && strcmp(arg_tokens[0], "fg") == 0) {
            exec_fg(arg_tokens);
            return;
        }
        if (n_cmds == 1 && strcmp(arg_tokens[0], "bg") == 0) {
            exec_bg(arg_tokens);
            return;
        }
        // These can run in child
        int is_builtin = 0;
        if (strcmp(arg_tokens[0], "pwd") == 0) is_builtin = 1;
        if (strcmp(arg_tokens[0], "jobs") == 0) is_builtin = 1;

        pid_t pid = fork();
        if (pid == 0) {
            // Child Process
            
            // Handle Pipes
            if (i != 0) { // Not first command, get input from prev pipe
                dup2(pipefd[(i - 1) * 2], STDIN_FILENO);
            }
            if (i != n_cmds - 1) { // Not last command, output to next pipe
                dup2(pipefd[i * 2 + 1], STDOUT_FILENO);
            }

            // Close all pipes in child
            for (int j = 0; j < 2 * (n_cmds - 1); j++) {
                close(pipefd[j]);
            }

            // Execute
            if (strcmp(arg_tokens[0], "pwd") == 0) {
                exec_pwd();
                exit(last_exit_status);
            } else if (strcmp(arg_tokens[0], "jobs") == 0) {
                exec_jobs();
                exit(last_exit_status);
            } else {
                execvp(arg_tokens[0], arg_tokens);
                perror("msh");
                exit(127);
            }
        } else if (pid < 0) {
            perror("fork");
            return;
        } else {
            pids[cmd_idx++] = pid;
            if (n_cmds == 1 && !is_bg) {
                foreground_pid = pid; // Track for signals
            }
        }
    }

    // Parent closes pipes
    for (int i = 0; i < 2 * (n_cmds - 1); i++) {
        close(pipefd[i]);
    }

    // Handling Waits
    if (!is_bg) {
        int status;
        for (int i = 0; i < n_cmds; i++) {
            waitpid(pids[i], &status, WUNTRACED);
            if (i == n_cmds - 1) { // Capture status of last command in pipe
                 if (WIFSTOPPED(status)) {
                    printf("\n");
                    add_job(pids[i], line, 1); // Stopped
                } else if (WIFEXITED(status)) {
                    last_exit_status = WEXITSTATUS(status);
                }
            }
        }
        foreground_pid = -1;
    } else {
        // Background Process
        // Add only the last pid of a pipe chain to job list for simplicity in this context
        // (Real shells use process groups)
        add_job(pids[n_cmds-1], line, 0); 
    }
}

// --- Main Loop ---

int main(int argc, char *argv[]) {
    char input[MAX_CMD_LEN];
    
    // Setup Shell Info
    shell_pid = getpid();
    if (readlink("/proc/self/exe", shell_path, PATH_MAX) == -1) {
        strcpy(shell_path, "unknown");
    }

    // Setup Signals
    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);
    signal(SIGCHLD, handle_sigchld);

    while (1) {
        char *ps1 = getenv("PS1");
        if (!ps1) ps1 = "msh> ";
        printf("%s", ps1);
        
        if (fgets(input, MAX_CMD_LEN, stdin) == NULL) {
            // EOF (Ctrl+D)
            printf("\n");
            break;
        }

        // Remove newline
        input[strcspn(input, "\n")] = 0;

        if (strlen(input) == 0) continue;

        expand_variables(input);
        execute_pipeline(input);
    }

    return 0;
}
