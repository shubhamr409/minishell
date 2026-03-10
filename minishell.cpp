/*
 * minishell.cpp — A minimal Unix shell in C++
 *
 * Features:
 *  - Command execution (via fork/execvp)
 *  - I/O Redirection  (< input, > output, >> append)
 *  - Pipes            (cmd1 | cmd2 | cmd3 ...)
 *  - Background jobs  (cmd &)
 *  - Built-ins        (cd, pwd, exit, help)
 *  - Signal handling  (Ctrl+C doesn't kill the shell)
 *
 * Build:  g++ -std=c++17 -Wall -o minishell minishell.cpp
 * Run:    ./minishell
 */

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

using namespace std;

// ─────────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────────
static const string SHELL_NAME = "minishell";
static const string PROMPT     = "\033[1;32mminishell\033[0m> ";

// ─────────────────────────────────────────────
//  Data structures
// ─────────────────────────────────────────────

struct Command {
    vector<string> args;   // argv[0] = program name
    string inputFile;      // < file
    string outputFile;     // > file
    bool appendOutput = false;  // >> flag
    bool background   = false;  // & flag
};

// ─────────────────────────────────────────────
//  Signal handling
// ─────────────────────────────────────────────

// SIGCHLD handler — reap background zombie children
void sigchld_handler(int /*sig*/) {
    while (waitpid(-1, nullptr, WNOHANG) > 0);
}

// SIGINT handler — shell itself ignores Ctrl+C
void sigint_handler(int /*sig*/) {
    cout << "\n" << PROMPT << flush;
}

// ─────────────────────────────────────────────
//  Tokeniser
// ─────────────────────────────────────────────

vector<string> tokenise(const string& line) {
    vector<string> tokens;
    istringstream iss(line);
    string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

// ─────────────────────────────────────────────
//  Parser — splits token list on '|', builds Commands
// ─────────────────────────────────────────────

vector<Command> parse(const vector<string>& tokens) {
    vector<Command> cmds;
    Command current;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const string& t = tokens[i];

        if (t == "|") {
            cmds.push_back(current);
            current = Command{};
        } else if (t == "<") {
            if (i + 1 < tokens.size()) current.inputFile = tokens[++i];
        } else if (t == ">>") {
            if (i + 1 < tokens.size()) {
                current.outputFile   = tokens[++i];
                current.appendOutput = true;
            }
        } else if (t == ">") {
            if (i + 1 < tokens.size()) current.outputFile = tokens[++i];
        } else if (t == "&") {
            current.background = true;
        } else {
            current.args.push_back(t);
        }
    }
    if (!current.args.empty()) cmds.push_back(current);
    return cmds;
}

// ─────────────────────────────────────────────
//  Built-in commands
// ─────────────────────────────────────────────

// Returns true if command was handled as a built-in
bool handle_builtin(const Command& cmd) {
    if (cmd.args.empty()) return false;

    const string& prog = cmd.args[0];

    if (prog == "exit") {
        cout << "Bye!\n";
        exit(0);
    }

    if (prog == "cd") {
        const char* dir = (cmd.args.size() > 1) ? cmd.args[1].c_str() : getenv("HOME");
        if (!dir) dir = "/";
        if (chdir(dir) != 0) perror("cd");
        return true;
    }

    if (prog == "pwd") {
        char buf[4096];
        if (getcwd(buf, sizeof(buf))) cout << buf << "\n";
        else perror("pwd");
        return true;
    }

    if (prog == "help") {
        cout
            << "─────────────────────────────────────────\n"
            << "  " << SHELL_NAME << " — built-in commands\n"
            << "─────────────────────────────────────────\n"
            << "  cd [dir]    Change directory\n"
            << "  pwd         Print working directory\n"
            << "  exit        Exit the shell\n"
            << "  help        Show this help\n\n"
            << "  Redirection : cmd < in.txt > out.txt >> append.txt\n"
            << "  Pipes       : cmd1 | cmd2 | cmd3\n"
            << "  Background  : cmd &\n"
            << "─────────────────────────────────────────\n";
        return true;
    }

    return false;
}

// ─────────────────────────────────────────────
//  Executor — runs a single command in a child process
//  inFd / outFd: pipe file descriptors (-1 = use stdio)
// ─────────────────────────────────────────────

void execute_single(const Command& cmd, int inFd, int outFd, bool background) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        // ── Child process ──

        // Restore default SIGINT so foreground children can be killed with Ctrl+C
        signal(SIGINT, SIG_DFL);

        // Pipe plumbing
        if (inFd != STDIN_FILENO) {
            dup2(inFd,  STDIN_FILENO);
            close(inFd);
        }
        if (outFd != STDOUT_FILENO) {
            dup2(outFd, STDOUT_FILENO);
            close(outFd);
        }

        // I/O redirection from files
        if (!cmd.inputFile.empty()) {
            int fd = open(cmd.inputFile.c_str(), O_RDONLY);
            if (fd < 0) { perror("open input"); exit(1); }
            dup2(fd, STDIN_FILENO); close(fd);
        }
        if (!cmd.outputFile.empty()) {
            int flags = O_WRONLY | O_CREAT | (cmd.appendOutput ? O_APPEND : O_TRUNC);
            int fd = open(cmd.outputFile.c_str(), flags, 0644);
            if (fd < 0) { perror("open output"); exit(1); }
            dup2(fd, STDOUT_FILENO); close(fd);
        }

        // Build argv
        vector<char*> argv;
        for (auto& a : cmd.args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        // If we reach here, exec failed
        cerr << SHELL_NAME << ": " << cmd.args[0] << ": command not found\n";
        exit(127);
    }

    // ── Parent process ──
    if (!background) waitpid(pid, nullptr, 0);
    else cout << "[bg] pid " << pid << "\n";
}

// ─────────────────────────────────────────────
//  Pipeline executor
// ─────────────────────────────────────────────

void execute_pipeline(vector<Command>& cmds) {
    if (cmds.empty()) return;

    // Single command — check built-ins first
    if (cmds.size() == 1) {
        if (!handle_builtin(cmds[0]))
            execute_single(cmds[0], STDIN_FILENO, STDOUT_FILENO, cmds[0].background);
        return;
    }

    // Multiple commands — wire up pipes
    int n = (int)cmds.size();
    vector<int> pipeFds(2 * (n - 1));

    for (int i = 0; i < n - 1; ++i) {
        if (pipe(&pipeFds[2 * i]) < 0) { perror("pipe"); return; }
    }

    for (int i = 0; i < n; ++i) {
        int inFd  = (i == 0)     ? STDIN_FILENO  : pipeFds[2 * (i - 1)];
        int outFd = (i == n - 1) ? STDOUT_FILENO : pipeFds[2 * i + 1];

        execute_single(cmds[i], inFd, outFd, false);

        // Close pipe ends we no longer need in parent
        if (i > 0)     close(pipeFds[2 * (i - 1)]);
        if (i < n - 1) close(pipeFds[2 * i + 1]);
    }

    // Wait for all children in the pipeline
    for (int i = 0; i < n; ++i) waitpid(-1, nullptr, 0);
}

// ─────────────────────────────────────────────
//  Main REPL
// ─────────────────────────────────────────────

int main() {
    // Register signal handlers
    signal(SIGINT,  sigint_handler);
    signal(SIGCHLD, sigchld_handler);

    cout << SHELL_NAME << " — type 'help' for usage, 'exit' to quit\n";

    string line;
    while (true) {
        cout << PROMPT << flush;

        if (!getline(cin, line)) {
            cout << "\n";
            break; // EOF (Ctrl+D)
        }

        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == string::npos) continue;
        line = line.substr(start);
        if (line.empty() || line[0] == '#') continue;

        auto tokens = tokenise(line);
        if (tokens.empty()) continue;

        auto cmds = parse(tokens);
        execute_pipeline(cmds);
    }

    return 0;
}