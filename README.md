# Mini Shell — Unix Shell Implementation in C++

A lightweight Unix shell built in C++17 supporting core shell features
including command execution, I/O redirection, multi-stage pipelines,
and background process management.

## Features

- Command execution via `fork` / `execvp`
- I/O Redirection — `<` (input), `>` (output), `>>` (append)
- Multi-stage Pipelines — `cmd1 | cmd2 | cmd3`
- Background Processes — `cmd &`
- Built-in Commands — `cd`, `pwd`, `help`, `exit`
- Signal Handling — `SIGINT` (Ctrl+C) and `SIGCHLD` (zombie reaping)

## Requirements

| Platform | Requirement |
|----------|-------------|
| Linux    | g++ 7+, make |
| macOS    | Xcode CLT (`xcode-select --install`) |
| Windows  | WSL2 (Ubuntu recommended) |

## Build & Run

### Linux / macOS
```bash
# Clone the repository
git clone https://github.com/shubhamr409/minishell.git
cd minishell

# Build
make

# Run
./minishell
```

### Windows (WSL2)
```bash
# Install WSL2 with Ubuntu from Microsoft Store, then:
git clone https://github.com/shubhamr409/minishell.git
cd minishell
make
./minishell
```

## Usage Examples
```bash
# Basic command
minishell> ls -la

# I/O Redirection
minishell> ls > output.txt
minishell> cat < output.txt
minishell> echo "hello" >> log.txt

# Pipelines
minishell> ls | grep .cpp
minishell> cat file.txt | sort | uniq

# Background process
minishell> sleep 5 &

# Built-ins
minishell> cd /home
minishell> pwd
minishell> help
minishell> exit
```

## Implementation Details

| Component | Details |
|-----------|---------|
| **Execution** | `fork()` + `execvp()` for child process creation |
| **Pipes** | Dynamic pipe array with `dup2()` for FD wiring |
| **Redirection** | `open()` + `dup2()` in child before `exec` |
| **Signals** | `SIGINT` ignored by shell; `SIGCHLD` reaps zombies |
| **Built-ins** | Handled in parent process before `fork` |

## Build Without Make
```bash
g++ -std=c++17 -Wall -o minishell minishell.cpp
./minishell
```