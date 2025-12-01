// Feature test macros must be defined before any includes
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "signals.h"
#include "jobs.h"
#include "utils.h"
#include <signal.h>
#include <sys/wait.h>

// SIGCHLD handler - reap zombie processes
static void sigchld_handler(int sig) {
    (void)sig; // Unused parameter
    
    int status;
    pid_t pid;
    
    // Reap all zombie processes
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        // Get process group ID of the process
        pid_t pgid = getpgid(pid);
        if (pgid == -1) {
            continue;
        }
        
        // Find job by process group
        // Only process jobs that are in the job table (background/stopped jobs)
        // Foreground processes should be waited for by the executor, not the handler
        Job *job = find_job_by_pgid(pgid);
        if (!job) {
            // Not a background/stopped job - skip it
            // The executor will handle waiting for foreground processes
            continue;
        }
        
        if (WIFSTOPPED(status)) {
            // Process was stopped (Ctrl+Z)
            update_job_status_by_pgid(pgid, JOB_STOPPED);
            printf("\n[%d]+  Stopped    %s\n", job->job_id, job->command);
            fflush(stdout);
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            // Process finished - mark as done
            // Only update status, don't remove yet (let jobs command show it)
            update_job_status_by_pgid(pgid, JOB_DONE);
        }
    }
}

// SIGTSTP handler - Note: SIGTSTP cannot be reliably caught/ignored
// It will always suspend the process. We set it to SIG_IGN to try to ignore it
// when the shell is in foreground, but this may not work on all systems.
// The standard approach is to ensure shell is foreground when waiting for input.

// SIGINT handler - kill foreground process
static void sigint_handler(int sig) {
    (void)sig; // Unused parameter
    
    // Get foreground process group
    pid_t fg_pgid = tcgetpgrp(STDIN_FILENO);
    if (fg_pgid == -1) {
        return;  // No foreground process
    }
    
    // Don't kill the shell itself
    if (fg_pgid == getpgrp()) {
        return;
    }
    
    // Send SIGINT to foreground process group
    kill(-fg_pgid, SIGINT);
}

// Initialize signal handlers
void init_signals(void) {
    struct sigaction sa;
    
    // SIGCHLD - reap zombies
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
    
    // SIGTSTP - suspend (Ctrl+Z)
    // Try to ignore SIGTSTP in the shell (may not work on all systems)
    // The key is ensuring shell is foreground when waiting for input
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTSTP, &sa, NULL);
    
    // SIGINT - interrupt (Ctrl+C)
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  // Restart interrupted system calls
    sigaction(SIGINT, &sa, NULL);
}

