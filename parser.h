#ifndef PARSER_H
#define PARSER_H

// Command structure to hold parsed command with redirections
typedef struct {
    char **argv;           // NULL-terminated argument array (command + args)
    char *input_file;      // For < redirection (NULL if none)
    char *output_file;     // For > or >> redirection (NULL if none)
    int append_mode;       // 1 for >>, 0 for > (only valid if output_file != NULL)
    int background;        // 1 if & at end, 0 otherwise
} Command;

// Pipeline structure to hold multiple commands connected by pipes
typedef struct {
    Command *commands;      // Array of commands
    int num_commands;       // Number of commands in pipeline
    int background;         // 1 if & at end, 0 otherwise
} Pipeline;

// Tokenize input string into array of tokens
// Returns number of tokens, or -1 on error
// tokens array must be freed by caller
int tokenize(char *input, char ***tokens);

// Parse tokens into Command structure
// Handles redirection operators: <, >, >>
// Returns 0 on success, -1 on error
// Command must be freed by caller using free_command()
int parse_command(char **tokens, Command *cmd);

// Parse tokens into Pipeline structure
// Handles pipe operator: |
// Splits tokens by | and creates Command for each part
// Returns 0 on success, -1 on error
// Pipeline must be freed by caller using free_pipeline()
int parse_pipeline(char **tokens, Pipeline *pipeline);

// Free Command structure
void free_command(Command *cmd);

// Free Pipeline structure
void free_pipeline(Pipeline *pipeline);

// Free token array allocated by tokenize
void free_tokens(char **tokens, int count);

#endif // PARSER_H
