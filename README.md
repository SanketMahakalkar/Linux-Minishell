# PROJECT PORTFOLIO: Linux Minishell
# Date: February 01, 2025
# Context: Emertxe Project / Embedded Systems Portfolio


================================================================================
 GITHUB README
================================================================================

# Linux Minishell

A custom command-line interpreter developed in **C** that mimics the core functionality of standard shells like Bash or Zsh. This project demonstrates a deep understanding of **Linux System Programming**, **Process Management**, and **Signal Handling**.

## 🚀 Features

* **Command Execution:** Supports execution of external system commands (e.g., `ls`, `ps`, `date`, `vim`).
* **Built-in Commands:** Implements shell-specific commands internally (e.g., `cd`, `exit`, `jobs`).
* **Input/Output Redirection:**
    * Output Redirection (`>`)
    * Input Redirection (`<`)
    * Append Mode (`>>`)
* **Piping:** Supports chaining multiple commands using pipes (`|`) to pass output from one process as input to another.
* **Job Control:**
    * Foreground and Background process management (`&`).
    * Signal Handling: Custom handlers for `SIGINT` (Ctrl+C) and `SIGTSTP` (Ctrl+Z).

## 🛠️ Technical Stack & Concepts

* **Language:** C
* **OS:** Linux (Ubuntu/Fedora)
* **System Calls:**
    * **Process Creation:** `fork()`, `vfork()`
    * **Execution:** `execvp()` family
    * **Process Waiting:** `wait()`, `waitpid()`
    * **File Descriptors:** `open()`, `close()`, `dup2()`
    * **Signals:** `signal()`, `sigaction()`

## 📂 Project Structure

```text
├── main.c           # Entry point and main loop
├── builtins.c       # Implementation of cd, exit, jobs
├── executor.c       # Logic for fork() and exec()
├── parser.c         # String tokenization and command parsing
├── signals.c        # Signal handler definitions
├── minishell.h      # Header file with function prototypes
├── Makefile         # Build script
└── README.md        # Documentation
