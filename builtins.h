#ifndef BUILTINS_H
#define BUILTINS_H

// Check if command is a built-in
// Returns 1 if built-in, 0 otherwise
int is_builtin(char *cmd);

// Execute built-in command
// argv: NULL-terminated array of arguments
// Returns exit status, or -1 on error
int execute_builtin(char **argv);

#endif // BUILTINS_H

