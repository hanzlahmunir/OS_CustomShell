// Feature test macros must be defined before any includes
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "utils.h"
#include "parser.h"
#include "executor.h"
#include "jobs.h"
#include "signals.h"
#include "history.h"

// Flag to track if we should continue running
static volatile int running = 1;

int main(void) {
    char *input = NULL;
    size_t input_size = 0;
    ssize_t nread;

    // Initialize job table
    init_jobs();
    
    // Initialize history
    init_history();
    
    // Initialize signal handlers
    init_signals();
    
    // Put shell in its own process group
    setpgid(0, 0);
    
    // Set shell as foreground process group
    if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
        // Ignore error if not a terminal
    }

    // Main REPL loop
    while (running) {
        // Clean up finished jobs before showing prompt
        cleanup_jobs();
        
        // Ensure shell's process group is foreground (important for getline)
        if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
            // Ignore error if not a terminal
        }
        
        // Display prompt
        printf("myshell> ");
        fflush(stdout);

        // Read input using getline (handles long lines automatically)
        // getline may be interrupted by signals - retry on EINTR
        nread = getline(&input, &input_size, stdin);
        
        // Handle EOF (Ctrl+D)
        if (nread == -1) {
            if (feof(stdin)) {
                // EOF - exit gracefully
                printf("\n");
                break;
            } else if (errno == EINTR) {
                // Interrupted by signal - clear error, restore foreground, and retry
                clearerr(stdin);
                // Ensure shell is still foreground
                if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
                    // Ignore error if not a terminal
                }
                continue;  // Retry getline
            } else {
                perror("getline");
                continue;
            }
        }

        // Remove newline if present
        if (nread > 0 && input[nread - 1] == '\n') {
            input[nread - 1] = '\0';
            nread--;
        }

        // Skip empty input
        if (nread == 0) {
            continue;
        }

        // Add to history (before processing, but after removing newline)
        add_to_history(input);

        // Tokenize input
        char **tokens = NULL;
        int token_count = tokenize(input, &tokens);

        if (token_count < 0) {
            fprintf(stderr, "myshell: tokenization error\n");
            continue;
        }

        if (token_count == 0) {
            // Empty line after tokenization
            free_tokens(tokens, 0);
            continue;
        }

        // Check if there are pipes
        int has_pipe = 0;
        for (int i = 0; tokens[i] != NULL; i++) {
            if (strcmp(tokens[i], "|") == 0) {
                has_pipe = 1;
                break;
            }
        }

        if (has_pipe) {
            // Parse and execute pipeline
            Pipeline pipeline;
            if (parse_pipeline(tokens, &pipeline) == -1) {
                // Error already printed by parse_pipeline
                free_tokens(tokens, token_count);
                continue;
            }

            // Execute pipeline
            (void)execute_pipeline(&pipeline);  // Status ignored for now

            // Check if exit command was executed (check first command)
            if (pipeline.num_commands > 0 && 
                pipeline.commands[0].argv && 
                pipeline.commands[0].argv[0] && 
                strcmp(pipeline.commands[0].argv[0], "exit") == 0) {
                // exit command will have already called exit(), but just in case
                running = 0;
            }

            // Free pipeline and tokens
            free_pipeline(&pipeline);
            free_tokens(tokens, token_count);
        } else {
            // Parse command with redirections (no pipes)
            Command cmd;
            if (parse_command(tokens, &cmd) == -1) {
                // Error already printed by parse_command
                free_tokens(tokens, token_count);
                continue;
            }

            // Execute command
            (void)execute_command(&cmd);  // Status ignored for now

            // Check if exit command was executed
            if (cmd.argv && cmd.argv[0] && strcmp(cmd.argv[0], "exit") == 0) {
                // exit command will have already called exit(), but just in case
                running = 0;
            }

            // Free command and tokens
            free_command(&cmd);
            free_tokens(tokens, token_count);
        }
    }

    // Clean up
    free(input);

    return 0;
}

