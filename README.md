# Custom Unix Shell (myshell)

A custom Unix shell implementation in C, demonstrating deep understanding of operating systems concepts through implementation of core shell functionality from scratch.

## Phase 1: Basic Shell ✅

### Features Implemented

- **Basic REPL Loop**: Read-Eval-Print loop with prompt display
- **Input Handling**: Uses `getline()` for robust input reading, handles EOF (Ctrl+D)
- **Basic Tokenization**: Whitespace-separated token parsing
- **Built-in Commands**:
  - `cd [directory]` - Change directory (defaults to HOME if no argument)
  - `pwd` - Print current working directory
  - `exit [status]` - Exit shell with optional status code
- **External Command Execution**: Uses `fork()` + `execvp()` for external programs
- **Signal Handling**: Basic SIGINT (Ctrl+C) handling (doesn't kill shell)

## Phase 2: Custom Command Implementation ✅

### Features Implemented

- **Custom Built-in Commands** (implemented from scratch using system calls):
  - `echo [args] [-n]` - Print arguments (uses `write()` system call)
  - `mkdir [dirs...]` - Create directories (uses `mkdir()` system call)
  - `rmdir [dirs...]` - Remove empty directories (uses `rmdir()` system call)
  - `touch [files...]` - Create/update files (uses `open()` with `O_CREAT`)
  - `rm [-r] [-f] [files...]` - Remove files/directories (uses `unlink()` and recursive `rmdir()`)
  - `cat [files...]` - Concatenate files (uses `open/read/write` with 4KB buffer)
  - `ls [-a] [dirs...]` - List directory contents (uses `opendir/readdir/stat`, color-coded)
  
All these commands run directly in the shell process (no fork/exec) and demonstrate deep OS knowledge through direct system call usage.

## Phase 3: Advanced Parsing ✅

### Features Implemented

- **Advanced Tokenization** with state machine:
  - **Single Quotes**: Literal strings, no escape processing
    - Example: `echo 'Hello World'` → single token with space
  - **Double Quotes**: Processed strings with escape characters
    - Example: `echo "Line 1\nLine 2"` → newline is processed
  - **Escape Characters**: `\n`, `\t`, `\\`, `\"`, `\'`, `\r`, `\0`
    - Only processed inside double quotes
  - **Error Handling**: Detects and reports unterminated quotes
  - **Space Preservation**: Spaces within quotes are part of the token

## Phase 4: I/O Redirection ✅

### Features Implemented

- **Input Redirection** (`<`): Read from file instead of stdin
  - Example: `cat < input.txt`
- **Output Redirection** (`>`): Write to file (truncates if exists)
  - Example: `ls > files.txt`
- **Append Redirection** (`>>`): Append to file
  - Example: `echo "line" >> file.txt`
- **Combined Redirections**: Both input and output
  - Example: `cat < input.txt > output.txt`
- **File Descriptor Management**: Proper use of `dup2()`, `open()`, `close()`
- **Error Handling**: Syntax errors, missing files, multiple redirections

## Phase 5: Piping ✅

### Features Implemented

- **Single Pipe** (`|`): Connect two commands
  - Example: `ls | grep txt`
- **Multiple Pipes**: Chain multiple commands
  - Example: `cat file.txt | grep error | sort | uniq`
- **Combined with Redirections**: Pipes + I/O redirection
  - Example: `cat < input.txt | sort > output.txt`
- **Process Management**: Fork for each command, proper file descriptor handling
- **Pipe Creation**: Uses `pipe()` system call
- **File Descriptor Management**: Critical closing of unused pipe FDs to prevent deadlocks

## Phase 6: Job Control and Background Processes ✅

### Features Implemented

- **Background Processes** (`&`): Run commands in background
  - Example: `sleep 10 &`
- **Job Control Commands**: `jobs`, `fg`, `bg`
  - `jobs` - List all background/stopped jobs
  - `fg [job_id]` - Bring job to foreground
  - `bg [job_id]` - Resume stopped job in background
- **Process Groups**: Each command/pipeline gets its own process group
- **Signal Handling**: SIGCHLD (reap zombies), SIGTSTP (Ctrl+Z), SIGINT (Ctrl+C)
- **Foreground/Background Control**: Proper terminal and process group management

## Phase 7: Command History ✅

### Features Implemented

- **Command History Storage**: Automatically stores last 1000 commands
- **History Built-in**: `history` command lists all stored commands
- **History Management**: Prevents duplicate consecutive commands
- **Circular Buffer**: Efficient storage using circular buffer

## Phase 8: Environment Variables ✅

### Features Implemented

- **Export Command**: `export VAR=value` - Set environment variables
- **Unset Command**: `unset VAR` - Remove environment variables
- **Variable Expansion**: `$HOME`, `${VAR}` - Expand variables in commands
  - Works in double quotes: `echo "Home: $HOME"`
  - Works in normal state: `cd $HOME`
  - Not expanded in single quotes: `echo '$HOME'` (literal)

### Compilation

```bash
make
```

This will create the `myshell` executable.

### Running

```bash
./myshell
```

### Example Usage

```bash
myshell> pwd
/home/user/project

myshell> cd /tmp
myshell> pwd
/tmp

myshell> ls
[lists files in /tmp]

myshell> echo hello world
hello world

myshell> exit
```

### Testing Phase 1

Test the following scenarios:

1. **Basic built-ins**:
   ```bash
   myshell> pwd
   myshell> cd /tmp
   myshell> cd
   myshell> exit
   ```

2. **External commands**:
   ```bash
   myshell> ls
   myshell> echo hello
   myshell> cat /etc/passwd | head -5
   ```

3. **Error handling**:
   ```bash
   myshell> cd /nonexistent
   myshell> invalidcommand
   ```

4. **EOF handling**: Press Ctrl+D to exit gracefully

### Code Structure

```
myshell/
├── Makefile          # Build configuration
├── shell.c           # Main REPL loop
├── parser.c/h        # Tokenization
├── executor.c/h       # Command execution (fork/exec)
├── builtins.c/h       # Built-in commands (cd, pwd, exit)
├── utils.h           # Common includes and constants
└── README.md         # This file
```

### Project Status

- ✅ Phase 1: Basic shell with built-ins (cd, pwd, exit) and external command execution
- ✅ Phase 2: Custom command implementations (ls, cat, echo, mkdir, rmdir, touch, rm) from scratch
- ✅ Phase 3: Advanced parsing (quotes, escape characters, variable expansion)
- ✅ Phase 4: I/O redirection (>, <, >>)
- ✅ Phase 5: Piping (|)
- ✅ Phase 6: Job control and advanced signal handling
- ✅ Phase 7: Command history
- ✅ Phase 8: Environment variables (export, unset, variable expansion)

**All phases complete!** The shell is fully functional with all required features and enhancements implemented.

### Current Status

**Phase 1 & 2 Complete:**
- ✅ Basic shell functionality
- ✅ Built-in commands (cd, pwd, exit)
- ✅ Custom command implementations (echo, mkdir, touch, cat, ls)
- ✅ External command execution

**Implemented Features:**
- ✅ All core built-in commands: `cd`, `pwd`, `exit`, `echo`, `mkdir`, `rmdir`, `touch`, `rm`, `cat`, `ls`
- ✅ Job control: `jobs`, `fg`, `bg`
- ✅ Command history: `history`
- ✅ Environment variables: `export`, `unset`
- ✅ Variable expansion: `$HOME`, `$USER`, `${VAR}`, etc.
- ✅ Advanced parsing: quotes, escapes, variable expansion
- ✅ I/O redirection: `>`, `<`, `>>`
- ✅ Piping: single and multiple pipes
- ✅ Background processes: `&`
- ✅ Signal handling: Ctrl+C, Ctrl+Z

**Known Limitations:**
- No command substitution (`` `command` `` or `$(command)`)
- No stderr redirection (`2>`, `2>>`)
- No heredoc (`<<`)
- No arrow keys for history navigation (would require readline library)
- No tab completion (would require readline library)
- Simple job cleanup (DONE jobs remain until manually cleaned)

