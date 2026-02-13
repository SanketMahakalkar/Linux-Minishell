# Linux Minishell (msh)

**Project:** Linux Minishell (System Programming)
**Tech Stack:** C, POSIX API, Signals, IPC.

Architected a custom shell to interact directly with the Linux Kernel, demonstrating mastery of process lifecycle management and memory handling.

* Utilized `fork()` for child process creation and `execvp()` for command execution, while implementing `waitpid()` to strictly prevent zombie processes.
* Manipulated file descriptors using `pipe()` and `dup2()` to enable inter-process communication, allowing the chaining of multiple commands (e.g., `ls | grep`).
* Integrated custom signal handlers (`SIGINT`, `SIGTSTP`) to manage job control, allowing users to interrupt processes (Ctrl+C) and manage background jobs.

## 🛠️ Technical Stack & Concepts

* **Language:** C
* **OS:** Linux (Ubuntu/Fedora)
* **System Calls:**
    * **Process Creation:** `fork()`
    * **Execution:** `execvp()` family
    * **Process Waiting:** `wait()`, `waitpid()`
    * **File Descriptors:** `pipe()`, `close()`, `dup2()`
    * **Signals:** `signal()`

## 📂 Project Structure

```text
├── msh.c            # Complete source code (Main loop, Parser, Executor)
├── msh              # Compiled executable
└── README.md        # Documentation
