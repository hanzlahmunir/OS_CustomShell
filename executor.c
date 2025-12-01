// Feature test macros must be defined before any includes
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "executor.h"
#include "builtins.h"
#include "jobs.h"
#include "utils.h"
#include <fcntl.h>
#include <signal.h>
#include <termios.h>

// Execute a command with redirections
// cmd: Command structure with argv and redirection info
// Returns exit status of command, or -1 on error
int execute_command(Command *cmd) {
    if (!cmd || !cmd->argv || !cmd->argv[0]) {
        return -1;
    }

    // Save original file descriptors
    int stdin_fd = dup(STDIN_FILENO);
    int stdout_fd = dup(STDOUT_FILENO);
    if (stdin_fd == -1 || stdout_fd == -1) {
        perror("myshell: dup");
        return -1;
    }

    // Handle input redirection
    if (cmd->input_file != NULL) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd == -1) {
            fprintf(stderr, "myshell: %s: ", cmd->input_file);
            perror("");
            close(stdin_fd);
            close(stdout_fd);
            return 1;
        }
        // Redirect stdin to file
        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("myshell: dup2");
            close(fd);
            close(stdin_fd);
            close(stdout_fd);
            return -1;
        }
        close(fd);
    }

    // Handle output redirection
    if (cmd->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT;
        if (cmd->append_mode) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
        int fd = open(cmd->output_file, flags, 0644);
        if (fd == -1) {
            fprintf(stderr, "myshell: %s: ", cmd->output_file);
            perror("");
            // Restore original file descriptors
            dup2(stdin_fd, STDIN_FILENO);
            dup2(stdout_fd, STDOUT_FILENO);
            close(stdin_fd);
            close(stdout_fd);
            return 1;
        }
        // Redirect stdout to file
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("myshell: dup2");
            close(fd);
            // Restore original file descriptors
            dup2(stdin_fd, STDIN_FILENO);
            dup2(stdout_fd, STDOUT_FILENO);
            close(stdin_fd);
            close(stdout_fd);
            return -1;
        }
        close(fd);
    }

    // Execute command
    int status = -1;

    // Check if it's a built-in command
    if (is_builtin(cmd->argv[0])) {
        status = execute_builtin(cmd->argv);
    } else {
        // External command - use fork + exec
        pid_t pid = fork();

        if (pid == -1) {
            perror("myshell: fork");
            status = -1;
        } else if (pid == 0) {
            // Child process
            // Create new process group
            setpgid(0, 0);
            
            // If foreground, set as foreground process group
            if (!cmd->background) {
                if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
                    // Ignore error if not a terminal
                }
            } else {
                // Background process - redirect stdin to /dev/null
                // This prevents background processes from reading from terminal
                int fd = open("/dev/null", O_RDONLY);
                if (fd != -1) {
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
            }
            
            // Execute the command
            if (execvp(cmd->argv[0], cmd->argv) == -1) {
                fprintf(stderr, "myshell: %s: command not found\n", cmd->argv[0]);
                exit(1);
            }
            // execvp never returns on success, but compiler needs a return
            exit(1);  // Should never reach here
        } else {
            // Parent process
            // Create process group (setpgid in parent before child execs)
            setpgid(pid, pid);
            
            // Get the actual process group ID (might be different if child already set it)
            pid_t pgid = getpgid(pid);
            if (pgid == -1) {
                pgid = pid;  // Fallback to pid if getpgid fails
            }
            
            if (cmd->background) {
                // Background process - don't wait
                // Build command string for job table
                char cmd_str[MAX_INPUT_SIZE] = {0};
                for (int i = 0; cmd->argv[i] != NULL; i++) {
                    if (i > 0) strcat(cmd_str, " ");
                    strcat(cmd_str, cmd->argv[i]);
                }
                
                int job_id = add_job(pgid, cmd_str, JOB_RUNNING);
                if (job_id > 0) {
                    printf("[%d] %d\n", job_id, (int)pgid);
                    fflush(stdout);
                }
                
                // Ensure shell's process group is foreground for getline() to work
                if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
                    // Ignore error if not a terminal
                }
                
                status = 0;  // Return success immediately
            } else {
                // Foreground process - set as foreground process group and wait
                if (tcsetpgrp(STDIN_FILENO, pid) == -1) {
                    // Ignore error if not a terminal
                }
                
                int child_status;
                if (waitpid(pid, &child_status, WUNTRACED) == -1) {
                    perror("myshell: waitpid");
                    status = -1;
                } else {
                    // Return shell's process group to foreground
                    if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
                        // Ignore error if not a terminal
                    }
                    
                    // Return exit status of child
                    if (WIFEXITED(child_status)) {
                        status = WEXITSTATUS(child_status);
                    } else if (WIFSIGNALED(child_status)) {
                        status = 128 + WTERMSIG(child_status);
                    } else if (WIFSTOPPED(child_status)) {
                        // Process was stopped - add to job table
                        char cmd_str[MAX_INPUT_SIZE] = {0};
                        for (int i = 0; cmd->argv[i] != NULL; i++) {
                            if (i > 0) strcat(cmd_str, " ");
                            strcat(cmd_str, cmd->argv[i]);
                        }
                        int job_id = add_job(pid, cmd_str, JOB_STOPPED);
                        if (job_id > 0) {
                            printf("\n[%d]+  Stopped    %s\n", job_id, cmd_str);
                        }
                        status = 0;
                    } else {
                        status = -1;
                    }
                }
            }
        }
    }

    // Restore original file descriptors
    if (dup2(stdin_fd, STDIN_FILENO) == -1) {
        perror("myshell: dup2 restore stdin");
    }
    if (dup2(stdout_fd, STDOUT_FILENO) == -1) {
        perror("myshell: dup2 restore stdout");
    }
    close(stdin_fd);
    close(stdout_fd);

    return status;
}

// Execute a pipeline of commands
// pipeline: Pipeline structure with multiple commands
// Returns exit status of last command, or -1 on error
int execute_pipeline(Pipeline *pipeline) {
    if (!pipeline || pipeline->num_commands == 0) {
        return -1;
    }

    // Single command - no pipes needed
    if (pipeline->num_commands == 1) {
        return execute_command(&pipeline->commands[0]);
    }

    // Multiple commands - need pipes
    int num_pipes = pipeline->num_commands - 1;
    int (*pipe_fds)[2] = malloc(num_pipes * sizeof(int[2]));
    if (!pipe_fds) {
        perror("malloc");
        return -1;
    }

    // Create all pipes
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipe_fds[i]) == -1) {
            perror("myshell: pipe");
            // Close already created pipes
            for (int j = 0; j < i; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            free(pipe_fds);
            return -1;
        }
    }

    // Save original stdin/stdout for restoration
    int stdin_fd = dup(STDIN_FILENO);
    int stdout_fd = dup(STDOUT_FILENO);
    if (stdin_fd == -1 || stdout_fd == -1) {
        perror("myshell: dup");
        // Close all pipes
        for (int i = 0; i < num_pipes; i++) {
            close(pipe_fds[i][0]);
            close(pipe_fds[i][1]);
        }
        free(pipe_fds);
        return -1;
    }

    pid_t *pids = malloc(pipeline->num_commands * sizeof(pid_t));
    if (!pids) {
        perror("malloc");
        // Close all pipes
        for (int i = 0; i < num_pipes; i++) {
            close(pipe_fds[i][0]);
            close(pipe_fds[i][1]);
        }
        free(pipe_fds);
        close(stdin_fd);
        close(stdout_fd);
        return -1;
    }

    // Fork and execute each command
    pid_t pipeline_pgid = 0;  // Process group ID for entire pipeline
    
    for (int i = 0; i < pipeline->num_commands; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("myshell: fork");
            // Kill already forked processes
            for (int j = 0; j < i; j++) {
                kill(pids[j], SIGTERM);
            }
            // Close all pipes
            for (int j = 0; j < num_pipes; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            free(pipe_fds);
            free(pids);
            close(stdin_fd);
            close(stdout_fd);
            return -1;
        }

        if (pid == 0) {
            // Child process
            // Create/join process group (first process creates, others join)
            if (i == 0) {
                // First process - create new process group
                setpgid(0, 0);
            }
            // For other processes, the parent will set the process group for us
            // via setpgid(pid, pipeline_pgid) call below
            
            // If foreground, set as foreground process group (only first process)
            if (!pipeline->background && i == 0) {
                if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
                    // Ignore error if not a terminal
                }
            }

            // Set up stdin
            if (i == 0) {
                // First command - check for input redirection
                if (pipeline->commands[i].input_file != NULL) {
                    int fd = open(pipeline->commands[i].input_file, O_RDONLY);
                    if (fd == -1) {
                        fprintf(stderr, "myshell: %s: ", pipeline->commands[i].input_file);
                        perror("");
                        exit(1);
                    }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                } else if (pipeline->background) {
                    // Background process - redirect stdin to /dev/null
                    int fd = open("/dev/null", O_RDONLY);
                    if (fd != -1) {
                        dup2(fd, STDIN_FILENO);
                        close(fd);
                    }
                }
                // If foreground and no input redirection, stdin stays as is (from terminal)
            } else {
                // Not first command - read from previous pipe
                dup2(pipe_fds[i - 1][0], STDIN_FILENO);
            }

            // Set up stdout
            if (i == pipeline->num_commands - 1) {
                // Last command - check for output redirection
                if (pipeline->commands[i].output_file != NULL) {
                    int flags = O_WRONLY | O_CREAT;
                    if (pipeline->commands[i].append_mode) {
                        flags |= O_APPEND;
                    } else {
                        flags |= O_TRUNC;
                    }
                    int fd = open(pipeline->commands[i].output_file, flags, 0644);
                    if (fd == -1) {
                        fprintf(stderr, "myshell: %s: ", pipeline->commands[i].output_file);
                        perror("");
                        exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                // If no output redirection, stdout stays as is (to terminal)
            } else {
                // Not last command - write to next pipe
                dup2(pipe_fds[i][1], STDOUT_FILENO);
            }

            // Close ALL pipe file descriptors in child
            // CRITICAL: After dup2, the pipe is accessible via stdin/stdout
            // Closing the original pipe fds is necessary so that:
            // 1. When a process exits, EOF is properly sent
            // 2. No file descriptor leaks occur
            // 3. The pipe works correctly
            for (int j = 0; j < num_pipes; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }

            // Set stdout to unbuffered if it's a pipe (not last command or has output redirection)
            // This ensures data is written immediately, not buffered
            if (i < pipeline->num_commands - 1 || pipeline->commands[i].output_file != NULL) {
                setvbuf(stdout, NULL, _IONBF, 0);
            }

            // Execute the command
            if (is_builtin(pipeline->commands[i].argv[0])) {
                int status = execute_builtin(pipeline->commands[i].argv);
                exit(status);
            } else {
                if (execvp(pipeline->commands[i].argv[0], pipeline->commands[i].argv) == -1) {
                    fprintf(stderr, "myshell: %s: command not found\n", 
                           pipeline->commands[i].argv[0]);
                    exit(1);
                }
                // execvp never returns on success
                exit(1);
            }
        } else {
            // Parent process - save pid
            pids[i] = pid;
            
            // Set process group (first process creates, others join)
            if (i == 0) {
                pipeline_pgid = pid;
                setpgid(pid, pid);
            } else {
                setpgid(pid, pipeline_pgid);
            }
        }
    }

    // Parent process - close all pipe file descriptors
    for (int i = 0; i < num_pipes; i++) {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }
    free(pipe_fds);

    if (pipeline->background) {
        // Background pipeline - don't wait
        // Build command string for job table
        char cmd_str[MAX_INPUT_SIZE] = {0};
        for (int i = 0; i < pipeline->num_commands; i++) {
            if (i > 0) strcat(cmd_str, " | ");
            for (int j = 0; pipeline->commands[i].argv[j] != NULL; j++) {
                if (j > 0) strcat(cmd_str, " ");
                strcat(cmd_str, pipeline->commands[i].argv[j]);
            }
        }
        
        int job_id = add_job(pipeline_pgid, cmd_str, JOB_RUNNING);
        if (job_id > 0) {
            printf("[%d] %d\n", job_id, (int)pipeline_pgid);
            fflush(stdout);
        }
        
        // Ensure shell's process group is foreground for getline() to work
        if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
            // Ignore error if not a terminal
        }
        
        free(pids);
        close(stdin_fd);
        close(stdout_fd);
        return 0;  // Return success immediately
    }

    // Foreground pipeline - set as foreground process group and wait
    if (tcsetpgrp(STDIN_FILENO, pipeline_pgid) == -1) {
        // Ignore error if not a terminal
    }

    // Wait for all child processes in the pipeline
    // Wait for the last process first (it's the one that produces final output)
    // Then wait for others to clean up
    int last_status = 0;
    
    // First, wait for the last process (which produces the final output)
    if (pipeline->num_commands > 0) {
        int status;
        pid_t last_pid = pids[pipeline->num_commands - 1];
        pid_t waited_pid = waitpid(last_pid, &status, WUNTRACED);
        
        if (waited_pid == last_pid) {
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                last_status = 128 + WTERMSIG(status);
            } else if (WIFSTOPPED(status)) {
                // Pipeline was stopped - add to job table
                char cmd_str[MAX_INPUT_SIZE] = {0};
                for (int j = 0; j < pipeline->num_commands; j++) {
                    if (j > 0) strcat(cmd_str, " | ");
                    for (int k = 0; pipeline->commands[j].argv[k] != NULL; k++) {
                        if (k > 0) strcat(cmd_str, " ");
                        strcat(cmd_str, pipeline->commands[j].argv[k]);
                    }
                }
                int job_id = add_job(pipeline_pgid, cmd_str, JOB_STOPPED);
                if (job_id > 0) {
                    printf("\n[%d]+  Stopped    %s\n", job_id, cmd_str);
                    fflush(stdout);
                }
                last_status = 0;
            }
        } else if (waited_pid == -1 && errno == ECHILD) {
            // Already reaped - that's fine
        }
    }
    
    // Now wait for any remaining processes (non-blocking to avoid hanging)
    for (int i = 0; i < pipeline->num_commands - 1; i++) {
        int status;
        pid_t waited_pid = waitpid(pids[i], &status, WNOHANG | WUNTRACED);
        
        // Don't care about status of intermediate processes - just reap them
        (void)waited_pid;
        (void)status;
    }
    
    // Return shell's process group to foreground
    if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
        // Ignore error if not a terminal
    }

    free(pids);

    // Restore original stdin/stdout
    dup2(stdin_fd, STDIN_FILENO);
    dup2(stdout_fd, STDOUT_FILENO);
    close(stdin_fd);
    close(stdout_fd);

    return last_status;
}
