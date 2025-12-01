#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

// Job status enumeration
typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} JobStatus;

// Job structure to track background/stopped processes
typedef struct {
    int job_id;            // Job number (1, 2, 3, ...)
    pid_t pgid;            // Process group ID
    char *command;         // Original command string
    JobStatus status;      // Current status
} Job;

// Initialize job table
void init_jobs(void);

// Add a new job to the table
// Returns job ID, or -1 on error
int add_job(pid_t pgid, const char *command, JobStatus status);

// Remove a job from the table
void remove_job(int job_id);

// Find job by job ID
// Returns pointer to job, or NULL if not found
Job *find_job(int job_id);

// Find job by process group ID
// Returns pointer to job, or NULL if not found
Job *find_job_by_pgid(pid_t pgid);

// Update job status
void update_job_status(int job_id, JobStatus status);

// Update job status by process group ID
void update_job_status_by_pgid(pid_t pgid, JobStatus status);

// Get all jobs (for jobs command)
// Returns number of jobs, fills jobs array (max MAX_JOBS)
int get_all_jobs(Job *jobs, int max_jobs);

// Get next available job ID
int get_next_job_id(void);

// Clean up finished jobs
void cleanup_jobs(void);

// Free job resources
void free_job(Job *job);

#endif // JOBS_H

