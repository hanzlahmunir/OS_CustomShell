// Feature test macros must be defined before any includes
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "parser.h"
#include "utils.h"
#include <ctype.h>

// Helper function to process escape characters in double quotes
// Returns the character value for escape sequences
static char process_escape(char c) {
    switch (c) {
        case 'n':  return '\n';
        case 't':  return '\t';
        case 'r':  return '\r';
        case '\\': return '\\';
        case '"':  return '"';
        case '\'': return '\'';
        case '0':  return '\0';
        default:   return c;  // Unknown escape, return as-is
    }
}

// Helper function to expand variable (e.g., $HOME, $USER)
// Returns expanded value or NULL if variable not found
// Caller must free the returned string
static char *expand_variable(const char *var_name) {
    if (!var_name || strlen(var_name) == 0) {
        return NULL;
    }
    
    char *value = getenv(var_name);
    if (value == NULL) {
        return NULL;
    }
    
    return strdup(value);
}

// Tokenize input string into array of tokens
// Handles:
//   - Single quotes (literal strings, no escapes)
//   - Double quotes (with escape characters)
//   - Escape characters (\n, \t, \\, \", \')
// Returns number of tokens, or -1 on error
int tokenize(char *input, char ***tokens) {
    if (!input || !tokens) {
        return -1;
    }

    // Allocate array for tokens
    *tokens = malloc(MAX_TOKENS * sizeof(char *));
    if (!*tokens) {
        perror("malloc");
        return -1;
    }

    // Allocate buffer for current token (with escape processing)
    char *token_buf = malloc(MAX_INPUT_SIZE);
    if (!token_buf) {
        perror("malloc");
        free(*tokens);
        *tokens = NULL;
        return -1;
    }

    int token_count = 0;
    int token_buf_pos = 0;
    enum {
        STATE_NORMAL,
        STATE_SINGLE_QUOTE,
        STATE_DOUBLE_QUOTE,
        STATE_ESCAPE
    } state = STATE_NORMAL;

    char *p = input;

    // Skip leading whitespace
    while (isspace(*p)) {
        p++;
    }

    // If input is empty or only whitespace
    if (*p == '\0') {
        free(*tokens);
        free(token_buf);
        *tokens = NULL;
        return 0;
    }

    while (*p != '\0' && token_count < MAX_TOKENS - 1) {
        switch (state) {
            case STATE_NORMAL:
                if (isspace(*p)) {
                    // Whitespace ends current token
                    if (token_buf_pos > 0) {
                        token_buf[token_buf_pos] = '\0';
                        (*tokens)[token_count] = strdup(token_buf);
                        if (!(*tokens)[token_count]) {
                            perror("strdup");
                            // Free already allocated tokens
                            for (int i = 0; i < token_count; i++) {
                                free((*tokens)[i]);
                            }
                            free(*tokens);
                            free(token_buf);
                            *tokens = NULL;
                            return -1;
                        }
                        token_count++;
                        token_buf_pos = 0;
                    }
                    // Skip whitespace
                    while (isspace(*p)) {
                        p++;
                    }
                } else if (*p == '\'') {
                    // Start of single-quoted string
                    state = STATE_SINGLE_QUOTE;
                    p++;
                } else if (*p == '"') {
                    // Start of double-quoted string
                    state = STATE_DOUBLE_QUOTE;
                    p++;
                } else if (*p == '$' && (isalnum(p[1]) || p[1] == '_' || p[1] == '{')) {
                    // Variable expansion: $VAR or ${VAR}
                    p++;  // Skip $
                    char var_name[256] = {0};
                    int var_pos = 0;
                    
                    // Handle ${VAR} syntax
                    if (*p == '{') {
                        p++;  // Skip {
                        while (*p != '\0' && *p != '}' && var_pos < 255) {
                            if (isalnum(*p) || *p == '_') {
                                var_name[var_pos++] = *p;
                                p++;
                            } else {
                                break;
                            }
                        }
                        if (*p == '}') {
                            p++;  // Skip }
                        }
                    } else {
                        // Handle $VAR syntax
                        while (*p != '\0' && (isalnum(*p) || *p == '_') && var_pos < 255) {
                            var_name[var_pos++] = *p;
                            p++;
                        }
                    }
                    
                    // Expand variable
                    char *var_value = expand_variable(var_name);
                    if (var_value) {
                        // Append expanded value to token buffer
                        int len = strlen(var_value);
                        for (int i = 0; i < len && token_buf_pos < MAX_INPUT_SIZE - 1; i++) {
                            token_buf[token_buf_pos++] = var_value[i];
                        }
                        free(var_value);
                    }
                    // If variable not found, nothing is added (like bash)
                } else if (*p == '\\') {
                    // Escape character in normal state (treat as literal backslash)
                    if (token_buf_pos < MAX_INPUT_SIZE - 1) {
                        token_buf[token_buf_pos++] = *p;
                    }
                    p++;
                } else {
                    // Regular character
                    if (token_buf_pos < MAX_INPUT_SIZE - 1) {
                        token_buf[token_buf_pos++] = *p;
                    }
                    p++;
                }
                break;

            case STATE_SINGLE_QUOTE:
                if (*p == '\'') {
                    // End of single-quoted string
                    state = STATE_NORMAL;
                    p++;
                } else {
                    // Literal character (no escapes in single quotes)
                    if (token_buf_pos < MAX_INPUT_SIZE - 1) {
                        token_buf[token_buf_pos++] = *p;
                    }
                    p++;
                }
                break;

            case STATE_DOUBLE_QUOTE:
                if (*p == '\\') {
                    // Escape sequence
                    state = STATE_ESCAPE;
                    p++;
                } else if (*p == '$' && (isalnum(p[1]) || p[1] == '_' || p[1] == '{')) {
                    // Variable expansion in double quotes: $VAR or ${VAR}
                    p++;  // Skip $
                    char var_name[256] = {0};
                    int var_pos = 0;
                    
                    // Handle ${VAR} syntax
                    if (*p == '{') {
                        p++;  // Skip {
                        while (*p != '\0' && *p != '}' && var_pos < 255) {
                            if (isalnum(*p) || *p == '_') {
                                var_name[var_pos++] = *p;
                                p++;
                            } else {
                                break;
                            }
                        }
                        if (*p == '}') {
                            p++;  // Skip }
                        }
                    } else {
                        // Handle $VAR syntax
                        while (*p != '\0' && (isalnum(*p) || *p == '_') && var_pos < 255) {
                            var_name[var_pos++] = *p;
                            p++;
                        }
                    }
                    
                    // Expand variable
                    char *var_value = expand_variable(var_name);
                    if (var_value) {
                        // Append expanded value to token buffer
                        int len = strlen(var_value);
                        for (int i = 0; i < len && token_buf_pos < MAX_INPUT_SIZE - 1; i++) {
                            token_buf[token_buf_pos++] = var_value[i];
                        }
                        free(var_value);
                    }
                    // If variable not found, nothing is added (like bash)
                } else if (*p == '"') {
                    // End of double-quoted string
                    state = STATE_NORMAL;
                    p++;
                } else {
                    // Regular character
                    if (token_buf_pos < MAX_INPUT_SIZE - 1) {
                        token_buf[token_buf_pos++] = *p;
                    }
                    p++;
                }
                break;

            case STATE_ESCAPE:
                // Process escape character
                if (token_buf_pos < MAX_INPUT_SIZE - 1) {
                    token_buf[token_buf_pos++] = process_escape(*p);
                }
                state = STATE_DOUBLE_QUOTE;
                p++;
                break;
        }
    }

    // Check for unterminated quotes
    if (state == STATE_SINGLE_QUOTE) {
        fprintf(stderr, "myshell: error: unterminated single quote\n");
        // Free already allocated tokens
        for (int i = 0; i < token_count; i++) {
            free((*tokens)[i]);
        }
        free(*tokens);
        free(token_buf);
        *tokens = NULL;
        return -1;
    }
    if (state == STATE_DOUBLE_QUOTE || state == STATE_ESCAPE) {
        fprintf(stderr, "myshell: error: unterminated double quote\n");
        // Free already allocated tokens
        for (int i = 0; i < token_count; i++) {
            free((*tokens)[i]);
        }
        free(*tokens);
        free(token_buf);
        *tokens = NULL;
        return -1;
    }

    // Handle last token if buffer has content
    if (token_buf_pos > 0) {
        token_buf[token_buf_pos] = '\0';
        (*tokens)[token_count] = strdup(token_buf);
        if (!(*tokens)[token_count]) {
            perror("strdup");
            // Free already allocated tokens
            for (int i = 0; i < token_count; i++) {
                free((*tokens)[i]);
            }
            free(*tokens);
            free(token_buf);
            *tokens = NULL;
            return -1;
        }
        token_count++;
    }

    // NULL terminate the array
    (*tokens)[token_count] = NULL;

    free(token_buf);
    return token_count;
}

// Parse tokens into Command structure
// Handles redirection operators: <, >, >>
// Returns 0 on success, -1 on error
int parse_command(char **tokens, Command *cmd) {
    if (!tokens || !cmd) {
        return -1;
    }

    // Initialize command structure
    cmd->argv = NULL;
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->append_mode = 0;
    cmd->background = 0;

    if (tokens[0] == NULL) {
        return -1;  // Empty command
    }

    // Allocate argv array
    char **argv = malloc(MAX_ARGS * sizeof(char *));
    if (!argv) {
        perror("malloc");
        return -1;
    }

    int argc = 0;
    int i = 0;

    // Parse tokens, handling redirections
    while (tokens[i] != NULL && argc < MAX_ARGS - 1) {
        if (strcmp(tokens[i], "<") == 0) {
            // Input redirection
            i++;
            if (tokens[i] == NULL) {
                fprintf(stderr, "myshell: syntax error near unexpected token '<'\n");
                free(argv);
                return -1;
            }
            if (cmd->input_file != NULL) {
                fprintf(stderr, "myshell: syntax error: multiple input redirections\n");
                free(argv);
                return -1;
            }
            cmd->input_file = strdup(tokens[i]);
            if (!cmd->input_file) {
                perror("strdup");
                free(argv);
                return -1;
            }
            i++;
        } else if (strcmp(tokens[i], ">") == 0) {
            // Output redirection (truncate)
            i++;
            if (tokens[i] == NULL) {
                fprintf(stderr, "myshell: syntax error near unexpected token '>'\n");
                free(argv);
                return -1;
            }
            if (cmd->output_file != NULL) {
                fprintf(stderr, "myshell: syntax error: multiple output redirections\n");
                free(argv);
                return -1;
            }
            cmd->output_file = strdup(tokens[i]);
            if (!cmd->output_file) {
                perror("strdup");
                free(argv);
                return -1;
            }
            cmd->append_mode = 0;
            i++;
        } else if (strcmp(tokens[i], ">>") == 0) {
            // Output redirection (append)
            i++;
            if (tokens[i] == NULL) {
                fprintf(stderr, "myshell: syntax error near unexpected token '>>'\n");
                free(argv);
                return -1;
            }
            if (cmd->output_file != NULL) {
                fprintf(stderr, "myshell: syntax error: multiple output redirections\n");
                free(argv);
                return -1;
            }
            cmd->output_file = strdup(tokens[i]);
            if (!cmd->output_file) {
                perror("strdup");
                free(argv);
                return -1;
            }
            cmd->append_mode = 1;
            i++;
        } else if (strcmp(tokens[i], "&") == 0) {
            // Background operator - must be last token
            if (tokens[i + 1] != NULL) {
                fprintf(stderr, "myshell: syntax error: & must be at end of command\n");
                free(argv);
                return -1;
            }
            cmd->background = 1;
            i++;  // Skip the &
            break;  // End of command
        } else {
            // Regular argument
            argv[argc++] = strdup(tokens[i]);
            if (!argv[argc - 1]) {
                perror("strdup");
                // Free already allocated arguments
                for (int j = 0; j < argc - 1; j++) {
                    free(argv[j]);
                }
                free(argv);
                if (cmd->input_file) free(cmd->input_file);
                if (cmd->output_file) free(cmd->output_file);
                return -1;
            }
            i++;
        }
    }

    // NULL terminate argv array
    argv[argc] = NULL;
    cmd->argv = argv;

    return 0;
}

// Parse tokens into Pipeline structure
// Handles pipe operator: |
// Splits tokens by | and creates Command for each part
// Returns 0 on success, -1 on error
int parse_pipeline(char **tokens, Pipeline *pipeline) {
    if (!tokens || !pipeline) {
        return -1;
    }

    // First, count how many commands (number of | + 1)
    // Also check for & at the end
    int pipe_count = 0;
    int has_background = 0;
    int last_token_idx = -1;
    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            pipe_count++;
        }
        last_token_idx = i;
    }
    
    // Check if last token is &
    if (last_token_idx >= 0 && strcmp(tokens[last_token_idx], "&") == 0) {
        has_background = 1;
    }

    int num_commands = pipe_count + 1;
    if (num_commands == 0) {
        return -1;
    }

    // Allocate array for commands
    Command *commands = malloc(num_commands * sizeof(Command));
    if (!commands) {
        perror("malloc");
        return -1;
    }

    // Initialize all commands
    for (int i = 0; i < num_commands; i++) {
        commands[i].argv = NULL;
        commands[i].input_file = NULL;
        commands[i].output_file = NULL;
        commands[i].append_mode = 0;
        commands[i].background = 0;
    }
    
    // Initialize pipeline
    pipeline->background = 0;

    // Split tokens by | and parse each segment
    int token_start = 0;
    int cmd_index = 0;

    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            // Found a pipe - parse command from token_start to i
            // Create a temporary token array for this command
            int cmd_token_count = i - token_start;
            if (cmd_token_count == 0) {
                fprintf(stderr, "myshell: syntax error near unexpected token '|'\n");
                // Free already allocated commands
                for (int j = 0; j < cmd_index; j++) {
                    free_command(&commands[j]);
                }
                free(commands);
                return -1;
            }

            char **cmd_tokens = malloc((cmd_token_count + 1) * sizeof(char *));
            if (!cmd_tokens) {
                perror("malloc");
                // Free already allocated commands
                for (int j = 0; j < cmd_index; j++) {
                    free_command(&commands[j]);
                }
                free(commands);
                return -1;
            }

            for (int j = 0; j < cmd_token_count; j++) {
                cmd_tokens[j] = tokens[token_start + j];
            }
            cmd_tokens[cmd_token_count] = NULL;

            // Parse this command
            if (parse_command(cmd_tokens, &commands[cmd_index]) == -1) {
                free(cmd_tokens);
                // Free already allocated commands
                for (int j = 0; j < cmd_index; j++) {
                    free_command(&commands[j]);
                }
                free(commands);
                return -1;
            }

            free(cmd_tokens);
            cmd_index++;
            token_start = i + 1;
        }
    }

    // Parse last command (after last |)
    int remaining_tokens = 0;
    for (int i = token_start; tokens[i] != NULL; i++) {
        remaining_tokens++;
    }

    if (remaining_tokens == 0) {
        fprintf(stderr, "myshell: syntax error near unexpected token '|'\n");
        // Free already allocated commands
        for (int j = 0; j < cmd_index; j++) {
            free_command(&commands[j]);
        }
        free(commands);
        return -1;
    }

    char **cmd_tokens = malloc((remaining_tokens + 1) * sizeof(char *));
    if (!cmd_tokens) {
        perror("malloc");
        // Free already allocated commands
        for (int j = 0; j < cmd_index; j++) {
            free_command(&commands[j]);
        }
        free(commands);
        return -1;
    }

    // Copy tokens, but skip & if it's the last one (already handled)
    int tokens_to_copy = remaining_tokens;
    if (has_background && remaining_tokens > 0 && 
        strcmp(tokens[token_start + remaining_tokens - 1], "&") == 0) {
        tokens_to_copy--;  // Don't include & in command parsing
    }
    
    for (int j = 0; j < tokens_to_copy; j++) {
        cmd_tokens[j] = tokens[token_start + j];
    }
    cmd_tokens[tokens_to_copy] = NULL;

    // Parse last command
    if (parse_command(cmd_tokens, &commands[cmd_index]) == -1) {
        free(cmd_tokens);
        // Free already allocated commands
        for (int j = 0; j < cmd_index; j++) {
            free_command(&commands[j]);
        }
        free(commands);
        return -1;
    }

    free(cmd_tokens);

    pipeline->commands = commands;
    pipeline->num_commands = num_commands;
    pipeline->background = has_background;

    return 0;
}

// Free Command structure
void free_command(Command *cmd) {
    if (!cmd) {
        return;
    }

    if (cmd->argv) {
        for (int i = 0; cmd->argv[i] != NULL; i++) {
            free(cmd->argv[i]);
        }
        free(cmd->argv);
    }

    if (cmd->input_file) {
        free(cmd->input_file);
    }

    if (cmd->output_file) {
        free(cmd->output_file);
    }
}

// Free Pipeline structure
void free_pipeline(Pipeline *pipeline) {
    if (!pipeline) {
        return;
    }

    if (pipeline->commands) {
        for (int i = 0; i < pipeline->num_commands; i++) {
            free_command(&pipeline->commands[i]);
        }
        free(pipeline->commands);
    }
}

// Free token array allocated by tokenize
void free_tokens(char **tokens, int count) {
    if (!tokens) {
        return;
    }

    for (int i = 0; i < count; i++) {
        free(tokens[i]);
    }
    free(tokens);
}
