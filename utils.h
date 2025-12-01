#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

// Maximum input size
#define MAX_INPUT_SIZE 4096
#define MAX_ARGS 64
#define MAX_TOKENS 128
#define MAX_JOBS 100  // Maximum number of jobs
#define MAX_HISTORY 1000  // Maximum history entries

#endif // UTILS_H

