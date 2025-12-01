#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

// Execute a command with redirections
// cmd: Command structure with argv and redirection info
// Returns exit status of command, or -1 on error
int execute_command(Command *cmd);

// Execute a pipeline of commands
// pipeline: Pipeline structure with multiple commands
// Returns exit status of last command, or -1 on error
int execute_pipeline(Pipeline *pipeline);

#endif // EXECUTOR_H
