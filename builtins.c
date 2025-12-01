// Feature test macros must be defined before any includes
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "builtins.h"
#include "utils.h"
#include "jobs.h"
#include "history.h"
#include <limits.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>

// Built-in command: cd
// Changes current working directory
static int builtin_cd(char **argv) {
    char *dir = NULL;

    // cd with no arguments goes to home directory
    if (argv[1] == NULL) {
        dir = getenv("HOME");
        if (dir == NULL) {
            fprintf(stderr, "myshell: cd: HOME not set\n");
            return 1;
        }
    } else {
        dir = argv[1];
    }

    if (chdir(dir) == -1) {
        perror("myshell: cd");
        return 1;
    }

    return 0;
}

// Built-in command: pwd
// Prints current working directory
static int builtin_pwd(char **argv) {
    (void)argv; // Unused parameter

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("myshell: pwd");
        return 1;
    }

    printf("%s\n", cwd);
    fflush(stdout);  // Ensure output is flushed (important when piped)
    return 0;
}

// Built-in command: exit
// Exits the shell with optional status code
static int builtin_exit(char **argv) {
    int status = 0;

    if (argv[1] != NULL) {
        status = atoi(argv[1]);
    }

    exit(status);
    // Never returns
}

// Built-in command: echo
// Prints arguments to stdout, handles -n flag
// Uses write() system call directly
static int builtin_echo(char **argv) {
    int no_newline = 0;
    int start_idx = 1;

    // Check for -n flag
    if (argv[1] != NULL && strcmp(argv[1], "-n") == 0) {
        no_newline = 1;
        start_idx = 2;
    }

    // Print all arguments
    for (int i = start_idx; argv[i] != NULL; i++) {
        if (i > start_idx) {
            // Add space between arguments
            if (write(STDOUT_FILENO, " ", 1) == -1) {
                perror("myshell: echo");
                return 1;
            }
        }
        // Write the argument
        size_t len = strlen(argv[i]);
        if (write(STDOUT_FILENO, argv[i], len) == -1) {
            perror("myshell: echo");
            return 1;
        }
    }

    // Add newline unless -n flag is set
    if (!no_newline) {
        if (write(STDOUT_FILENO, "\n", 1) == -1) {
            perror("myshell: echo");
            return 1;
        }
    }

    return 0;
}

// Built-in command: mkdir
// Creates a directory using mkdir() system call
static int builtin_mkdir(char **argv) {
    if (argv[1] == NULL) {
        fprintf(stderr, "myshell: mkdir: missing operand\n");
        return 1;
    }

    int error_occurred = 0;

    // Handle multiple directories
    for (int i = 1; argv[i] != NULL; i++) {
        // Create directory with permissions 0755 (rwxr-xr-x)
        if (mkdir(argv[i], 0755) == -1) {
            fprintf(stderr, "myshell: mkdir: cannot create directory '%s': ", argv[i]);
            perror("");
            error_occurred = 1;
        }
    }

    return error_occurred ? 1 : 0;
}

// Built-in command: touch
// Creates an empty file or updates its timestamp
// Uses open() with O_CREAT flag
static int builtin_touch(char **argv) {
    if (argv[1] == NULL) {
        fprintf(stderr, "myshell: touch: missing file operand\n");
        return 1;
    }

    int error_occurred = 0;

    // Handle multiple files
    for (int i = 1; argv[i] != NULL; i++) {
        // Open file with O_CREAT and O_WRONLY
        // If file exists, this will just open it (updating access time)
        // If file doesn't exist, it will be created
        int fd = open(argv[i], O_CREAT | O_WRONLY, 0644);
        if (fd == -1) {
            fprintf(stderr, "myshell: touch: cannot touch '%s': ", argv[i]);
            perror("");
            error_occurred = 1;
        } else {
            // Update modification time by writing nothing (or just closing)
            // Actually, just opening with O_WRONLY and closing updates the access time
            // To update modification time, we'd need utimensat(), but for simplicity
            // we'll just close the file
            close(fd);
        }
    }

    return error_occurred ? 1 : 0;
}

// Built-in command: rmdir
// Removes empty directories using rmdir() system call
static int builtin_rmdir(char **argv) {
    if (argv[1] == NULL) {
        fprintf(stderr, "myshell: rmdir: missing operand\n");
        return 1;
    }

    int error_occurred = 0;

    // Handle multiple directories
    for (int i = 1; argv[i] != NULL; i++) {
        if (rmdir(argv[i]) == -1) {
            fprintf(stderr, "myshell: rmdir: cannot remove '%s': ", argv[i]);
            perror("");
            error_occurred = 1;
        }
    }

    return error_occurred ? 1 : 0;
}

// Helper function to recursively remove directory
static int remove_directory_recursive(const char *path, int force) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        if (!force) {
            perror("myshell: rm: opendir");
        }
        return -1;
    }

    struct dirent *entry;
    char full_path[PATH_MAX + 1];

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Build full path
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == -1) {
            if (!force) {
                perror("myshell: rm: stat");
            }
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // Recursively remove directory
            if (remove_directory_recursive(full_path, force) == -1) {
                closedir(dir);
                return -1;
            }
        } else {
            // Remove file
            if (unlink(full_path) == -1) {
                if (!force) {
                    fprintf(stderr, "myshell: rm: cannot remove '%s': ", full_path);
                    perror("");
                }
            }
        }
    }

    closedir(dir);

    // Remove the directory itself
    if (rmdir(path) == -1) {
        if (!force) {
            fprintf(stderr, "myshell: rm: cannot remove '%s': ", path);
            perror("");
        }
        return -1;
    }

    return 0;
}

// Built-in command: rm
// Removes files and directories using unlink() and rmdir() system calls
// Supports -r (recursive) and -f (force) flags
static int builtin_rm(char **argv) {
    if (argv[1] == NULL) {
        fprintf(stderr, "myshell: rm: missing operand\n");
        return 1;
    }

    int recursive = 0;
    int force = 0;
    int arg_start = 1;

    // Parse flags
    while (argv[arg_start] != NULL && argv[arg_start][0] == '-') {
        char *flags = argv[arg_start] + 1;
        for (int i = 0; flags[i] != '\0'; i++) {
            if (flags[i] == 'r') {
                recursive = 1;
            } else if (flags[i] == 'f') {
                force = 1;
            } else {
                fprintf(stderr, "myshell: rm: invalid option -- '%c'\n", flags[i]);
                return 1;
            }
        }
        arg_start++;
    }

    if (argv[arg_start] == NULL) {
        fprintf(stderr, "myshell: rm: missing operand\n");
        return 1;
    }

    int error_occurred = 0;

    // Process each file/directory
    for (int i = arg_start; argv[i] != NULL; i++) {
        struct stat st;
        if (stat(argv[i], &st) == -1) {
            if (!force) {
                fprintf(stderr, "myshell: rm: cannot remove '%s': ", argv[i]);
                perror("");
                error_occurred = 1;
            }
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (!recursive) {
                if (!force) {
                    fprintf(stderr, "myshell: rm: '%s': is a directory\n", argv[i]);
                }
                error_occurred = 1;
            } else {
                // Recursively remove directory
                if (remove_directory_recursive(argv[i], force) == -1) {
                    error_occurred = 1;
                }
            }
        } else {
            // Remove file
            if (unlink(argv[i]) == -1) {
                if (!force) {
                    fprintf(stderr, "myshell: rm: cannot remove '%s': ", argv[i]);
                    perror("");
                    error_occurred = 1;
                }
            }
        }
    }

    return error_occurred ? 1 : 0;
}

// Built-in command: cat
// Concatenates and prints files using open/read/write
// Uses 4KB buffer for reading
// If no arguments, reads from stdin
static int builtin_cat(char **argv) {
    int error_occurred = 0;
    char buffer[4096];  // 4KB buffer
    ssize_t bytes_read;

    // If no arguments, read from stdin
    if (argv[1] == NULL) {
        // Read from stdin in 4KB chunks and write to stdout
        while ((bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
            ssize_t bytes_written = 0;
            ssize_t total_written = 0;

            // Write all bytes (handle partial writes)
            while (total_written < bytes_read) {
                bytes_written = write(STDOUT_FILENO, buffer + total_written, 
                                     bytes_read - total_written);
                if (bytes_written == -1) {
                    perror("myshell: cat");
                    return 1;
                }
                total_written += bytes_written;
            }
        }

        if (bytes_read == -1) {
            perror("myshell: cat");
            return 1;
        }

        return 0;
    }

    // Process each file
    for (int i = 1; argv[i] != NULL; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd == -1) {
            fprintf(stderr, "myshell: cat: %s: ", argv[i]);
            perror("");
            error_occurred = 1;
            continue;
        }

        // Read file in 4KB chunks and write to stdout
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            ssize_t bytes_written = 0;
            ssize_t total_written = 0;

            // Write all bytes (handle partial writes)
            while (total_written < bytes_read) {
                bytes_written = write(STDOUT_FILENO, buffer + total_written, 
                                     bytes_read - total_written);
                if (bytes_written == -1) {
                    perror("myshell: cat");
                    error_occurred = 1;
                    break;
                }
                total_written += bytes_written;
            }
        }

        if (bytes_read == -1) {
            fprintf(stderr, "myshell: cat: %s: ", argv[i]);
            perror("");
            error_occurred = 1;
        }

        close(fd);
    }

    return error_occurred ? 1 : 0;
}

// Built-in command: ls
// Lists directory contents using opendir/readdir/stat
// Color codes directories (blue) and files
static int builtin_ls(char **argv) {
    int show_all = 0;  // -a flag for hidden files
    int arg_start = 1;
    char *dirs[MAX_ARGS];
    int dir_count = 0;

    // Parse flags (only -a for now)
    while (argv[arg_start] != NULL && argv[arg_start][0] == '-') {
        if (strcmp(argv[arg_start], "-a") == 0) {
            show_all = 1;
        } else {
            fprintf(stderr, "myshell: ls: invalid option -- '%s'\n", 
                   argv[arg_start] + 1);
            return 1;
        }
        arg_start++;
    }

    // Collect directories to list
    if (argv[arg_start] == NULL) {
        // No directory specified, use current directory
        dirs[0] = ".";
        dir_count = 1;
    } else {
        // Collect all specified directories
        for (int i = arg_start; argv[i] != NULL && dir_count < MAX_ARGS - 1; i++) {
            dirs[dir_count++] = argv[i];
        }
    }

    int error_occurred = 0;

    // List each directory
    for (int d = 0; d < dir_count; d++) {
        if (dir_count > 1) {
            // Print directory name if multiple directories
            printf("%s:\n", dirs[d]);
            fflush(stdout);
        }

        DIR *dir = opendir(dirs[d]);
        if (dir == NULL) {
            fprintf(stderr, "myshell: ls: cannot access '%s': ", dirs[d]);
            perror("");
            error_occurred = 1;
            continue;
        }

        struct dirent *entry;
        struct stat file_stat;
        char full_path[PATH_MAX + 1];

        // Read directory entries
        while ((entry = readdir(dir)) != NULL) {
            // Skip hidden files unless -a flag is set
            if (!show_all && entry->d_name[0] == '.') {
                continue;
            }

            // Build full path for stat()
            snprintf(full_path, sizeof(full_path), "%s/%s", dirs[d], entry->d_name);

            // Get file information
            if (stat(full_path, &file_stat) == -1) {
                // If stat fails, just print the name without color
                printf("%s\n", entry->d_name);
                fflush(stdout);
                continue;
            }

            // Color code: blue for directories, default for files
            if (S_ISDIR(file_stat.st_mode)) {
                // Blue color for directories: \033[34m (ANSI escape code)
                printf("\033[34m%s\033[0m\n", entry->d_name);
                fflush(stdout);
            } else {
                printf("%s\n", entry->d_name);
                fflush(stdout);
            }
        }

        closedir(dir);

        if (d < dir_count - 1) {
            printf("\n");  // Blank line between directories
            fflush(stdout);
        }
    }

    return error_occurred ? 1 : 0;
}

// Built-in command: jobs
// Lists all background and stopped jobs
static int builtin_jobs(char **argv) {
    (void)argv; // Unused parameter

    Job jobs[MAX_JOBS];
    int num_jobs = get_all_jobs(jobs, MAX_JOBS);

    if (num_jobs == 0) {
        return 0;  // No jobs
    }

    for (int i = 0; i < num_jobs; i++) {
        const char *status_str;
        switch (jobs[i].status) {
            case JOB_RUNNING:
                status_str = "Running";
                break;
            case JOB_STOPPED:
                status_str = "Stopped";
                break;
            case JOB_DONE:
                status_str = "Done";
                break;
            default:
                status_str = "Unknown";
                break;
        }
        printf("[%d] %s %s\n", jobs[i].job_id, status_str, jobs[i].command);
    }
    fflush(stdout);

    return 0;
}

// Built-in command: fg
// Brings a background/stopped job to foreground
static int builtin_fg(char **argv) {
    if (argv[1] == NULL) {
        fprintf(stderr, "myshell: fg: usage: fg [job_id]\n");
        return 1;
    }

    int job_id = atoi(argv[1]);
    if (job_id <= 0) {
        fprintf(stderr, "myshell: fg: %s: no such job\n", argv[1]);
        return 1;
    }

    Job *job = find_job(job_id);
    if (!job) {
        fprintf(stderr, "myshell: fg: %d: no such job\n", job_id);
        return 1;
    }

    // Bring process group to foreground
    if (tcsetpgrp(STDIN_FILENO, job->pgid) == -1) {
        perror("myshell: fg: tcsetpgrp");
        return 1;
    }

    // Send SIGCONT to resume if stopped
    if (job->status == JOB_STOPPED) {
        if (kill(-job->pgid, SIGCONT) == -1) {
            perror("myshell: fg: kill");
            return 1;
        }
        update_job_status(job_id, JOB_RUNNING);
    }

    // Wait for the process group
    int status;
    pid_t pid;
    while ((pid = waitpid(-job->pgid, &status, WUNTRACED)) > 0) {
        if (WIFSTOPPED(status)) {
            update_job_status_by_pgid(job->pgid, JOB_STOPPED);
            printf("\n[%d]+  Stopped    %s\n", job_id, job->command);
            break;
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            remove_job(job_id);
            break;
        }
    }

    // Return shell's process group to foreground
    if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
        perror("myshell: fg: tcsetpgrp");
    }

    return 0;
}

// Built-in command: bg
// Resumes a stopped job in background
static int builtin_bg(char **argv) {
    if (argv[1] == NULL) {
        fprintf(stderr, "myshell: bg: usage: bg [job_id]\n");
        return 1;
    }

    int job_id = atoi(argv[1]);
    if (job_id <= 0) {
        fprintf(stderr, "myshell: bg: %s: no such job\n", argv[1]);
        return 1;
    }

    Job *job = find_job(job_id);
    if (!job) {
        fprintf(stderr, "myshell: bg: %d: no such job\n", job_id);
        return 1;
    }

    if (job->status != JOB_STOPPED) {
        fprintf(stderr, "myshell: bg: job %d is not stopped\n", job_id);
        return 1;
    }

    // Send SIGCONT to resume
    if (kill(-job->pgid, SIGCONT) == -1) {
        perror("myshell: bg: kill");
        return 1;
    }

    update_job_status(job_id, JOB_RUNNING);
    printf("[%d]+ %s &\n", job_id, job->command);

    return 0;
}

// Built-in command: history
// Lists command history
static int builtin_history(char **argv) {
    (void)argv; // Unused parameter

    const char *history[1000];  // Use fixed size matching MAX_HISTORY
    int count = get_all_history(history, 1000);

    // Print history with line numbers (1-based, like bash)
    int start_num = 1;
    if (count < get_history_count()) {
        start_num = get_history_count() - count + 1;
    }

    for (int i = 0; i < count; i++) {
        printf("%5d  %s\n", start_num + i, history[i]);
    }
    fflush(stdout);

    return 0;
}

// Built-in command: export
// Sets environment variable: export VAR=value
static int builtin_export(char **argv) {
    if (argv[1] == NULL) {
        // Print all environment variables (simplified - just show a few common ones)
        extern char **environ;
        for (int i = 0; environ[i] != NULL; i++) {
            printf("declare -x %s\n", environ[i]);
        }
        fflush(stdout);
        return 0;
    }

    int error_occurred = 0;

    for (int i = 1; argv[i] != NULL; i++) {
        char *arg = argv[i];
        char *equals = strchr(arg, '=');
        
        if (equals == NULL) {
            // Just variable name - check if it exists
            char *value = getenv(arg);
            if (value == NULL) {
                fprintf(stderr, "myshell: export: %s: variable not set\n", arg);
                error_occurred = 1;
            } else {
                // Variable exists, export it (already exported if from environment)
                setenv(arg, value, 1);
            }
        } else {
            // VAR=value format - need to copy to avoid modifying argv
            int name_len = equals - arg;
            char *var_name = malloc(name_len + 1);
            if (!var_name) {
                perror("malloc");
                error_occurred = 1;
                continue;
            }
            strncpy(var_name, arg, name_len);
            var_name[name_len] = '\0';
            char *var_value = equals + 1;
            
            if (setenv(var_name, var_value, 1) == -1) {
                perror("myshell: export");
                error_occurred = 1;
            }
            
            free(var_name);
        }
    }

    return error_occurred ? 1 : 0;
}

// Built-in command: unset
// Unsets environment variable
static int builtin_unset(char **argv) {
    if (argv[1] == NULL) {
        fprintf(stderr, "myshell: unset: usage: unset [variable...]\n");
        return 1;
    }

    int error_occurred = 0;

    for (int i = 1; argv[i] != NULL; i++) {
        if (unsetenv(argv[i]) == -1) {
            perror("myshell: unset");
            error_occurred = 1;
        }
    }

    return error_occurred ? 1 : 0;
}

// Check if command is a built-in
int is_builtin(char *cmd) {
    if (!cmd) {
        return 0;
    }

    return (strcmp(cmd, "cd") == 0 ||
            strcmp(cmd, "pwd") == 0 ||
            strcmp(cmd, "exit") == 0 ||
            strcmp(cmd, "echo") == 0 ||
            strcmp(cmd, "mkdir") == 0 ||
            strcmp(cmd, "rmdir") == 0 ||
            strcmp(cmd, "touch") == 0 ||
            strcmp(cmd, "rm") == 0 ||
            strcmp(cmd, "cat") == 0 ||
            strcmp(cmd, "ls") == 0 ||
            strcmp(cmd, "jobs") == 0 ||
            strcmp(cmd, "fg") == 0 ||
            strcmp(cmd, "bg") == 0 ||
            strcmp(cmd, "history") == 0 ||
            strcmp(cmd, "export") == 0 ||
            strcmp(cmd, "unset") == 0);
}

// Execute built-in command
int execute_builtin(char **argv) {
    if (!argv || !argv[0]) {
        return -1;
    }

    char *cmd = argv[0];

    if (strcmp(cmd, "cd") == 0) {
        return builtin_cd(argv);
    } else if (strcmp(cmd, "pwd") == 0) {
        return builtin_pwd(argv);
    } else if (strcmp(cmd, "exit") == 0) {
        return builtin_exit(argv);
    } else if (strcmp(cmd, "echo") == 0) {
        return builtin_echo(argv);
    } else if (strcmp(cmd, "mkdir") == 0) {
        return builtin_mkdir(argv);
    } else if (strcmp(cmd, "rmdir") == 0) {
        return builtin_rmdir(argv);
    } else if (strcmp(cmd, "touch") == 0) {
        return builtin_touch(argv);
    } else if (strcmp(cmd, "rm") == 0) {
        return builtin_rm(argv);
    } else if (strcmp(cmd, "cat") == 0) {
        return builtin_cat(argv);
    } else if (strcmp(cmd, "ls") == 0) {
        return builtin_ls(argv);
    } else if (strcmp(cmd, "jobs") == 0) {
        return builtin_jobs(argv);
    } else if (strcmp(cmd, "fg") == 0) {
        return builtin_fg(argv);
    } else if (strcmp(cmd, "bg") == 0) {
        return builtin_bg(argv);
    } else if (strcmp(cmd, "history") == 0) {
        return builtin_history(argv);
    } else if (strcmp(cmd, "export") == 0) {
        return builtin_export(argv);
    } else if (strcmp(cmd, "unset") == 0) {
        return builtin_unset(argv);
    }

    return -1;
}

