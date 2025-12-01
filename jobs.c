// Feature test macros must be defined before any includes
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "jobs.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

// Job table
static Job job_table[MAX_JOBS];
static int next_job_id = 1;
static int num_jobs = 0;

// Initialize job table
void init_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        job_table[i].job_id = 0;
        job_table[i].pgid = 0;
        job_table[i].command = NULL;
        job_table[i].status = JOB_DONE;
    }
    next_job_id = 1;
    num_jobs = 0;
}

// Add a new job to the table
int add_job(pid_t pgid, const char *command, JobStatus status) {
    if (num_jobs >= MAX_JOBS) {
        return -1;  // Table full
    }

    // Find empty slot
    int slot = -1;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].job_id == 0) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        return -1;  // No empty slot
    }

    job_table[slot].job_id = next_job_id++;
    job_table[slot].pgid = pgid;
    job_table[slot].command = strdup(command);
    if (!job_table[slot].command) {
        perror("strdup");
        return -1;
    }
    job_table[slot].status = status;
    num_jobs++;

    return job_table[slot].job_id;
}

// Remove a job from the table
void remove_job(int job_id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].job_id == job_id) {
            free(job_table[i].command);
            job_table[i].job_id = 0;
            job_table[i].pgid = 0;
            job_table[i].command = NULL;
            job_table[i].status = JOB_DONE;
            num_jobs--;
            return;
        }
    }
}

// Find job by job ID
Job *find_job(int job_id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].job_id == job_id) {
            return &job_table[i];
        }
    }
    return NULL;
}

// Find job by process group ID
Job *find_job_by_pgid(pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].job_id != 0 && job_table[i].pgid == pgid) {
            return &job_table[i];
        }
    }
    return NULL;
}

// Update job status
void update_job_status(int job_id, JobStatus status) {
    Job *job = find_job(job_id);
    if (job) {
        job->status = status;
    }
}

// Update job status by process group ID
void update_job_status_by_pgid(pid_t pgid, JobStatus status) {
    Job *job = find_job_by_pgid(pgid);
    if (job) {
        job->status = status;
    }
}

// Get all jobs (for jobs command)
int get_all_jobs(Job *jobs, int max_jobs) {
    int count = 0;
    for (int i = 0; i < MAX_JOBS && count < max_jobs; i++) {
        if (job_table[i].job_id != 0 && 
            job_table[i].status != JOB_DONE) {
            jobs[count] = job_table[i];
            // Don't duplicate command string, just copy pointer
            // Caller should not free it
            count++;
        }
    }
    return count;
}

// Get next available job ID
int get_next_job_id(void) {
    return next_job_id;
}

// Clean up finished jobs
void cleanup_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].job_id != 0 && job_table[i].status == JOB_DONE) {
            free(job_table[i].command);
            job_table[i].job_id = 0;
            job_table[i].pgid = 0;
            job_table[i].command = NULL;
            num_jobs--;
        }
    }
}

// Free job resources
void free_job(Job *job) {
    if (job && job->command) {
        free(job->command);
        job->command = NULL;
    }
}

