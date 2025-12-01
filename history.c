// Feature test macros must be defined before any includes
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "history.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

#define MAX_HISTORY 1000

static char *history_buffer[MAX_HISTORY];
static int history_count = 0;
static int history_next = 0;  // Next position to write

// Initialize history
void init_history(void) {
    for (int i = 0; i < MAX_HISTORY; i++) {
        history_buffer[i] = NULL;
    }
    history_count = 0;
    history_next = 0;
}

// Add command to history
void add_to_history(const char *command) {
    if (!command || strlen(command) == 0) {
        return;  // Don't store empty commands
    }

    // Skip if same as last command
    if (history_count > 0) {
        int last_idx = (history_next - 1 + MAX_HISTORY) % MAX_HISTORY;
        if (history_buffer[last_idx] && strcmp(history_buffer[last_idx], command) == 0) {
            return;  // Don't duplicate consecutive identical commands
        }
    }

    // Free old entry if overwriting
    if (history_buffer[history_next] != NULL) {
        free(history_buffer[history_next]);
    }

    // Store new command
    history_buffer[history_next] = strdup(command);
    if (!history_buffer[history_next]) {
        perror("strdup");
        return;
    }

    history_next = (history_next + 1) % MAX_HISTORY;
    if (history_count < MAX_HISTORY) {
        history_count++;
    }
}

// Get history entry by index (1-based, like bash)
const char *get_history_entry(int index) {
    if (index < 1 || index > history_count) {
        return NULL;
    }

    // Calculate position (most recent is history_count, oldest is 1)
    int pos = (history_next - history_count + index - 1 + MAX_HISTORY) % MAX_HISTORY;
    return history_buffer[pos];
}

// Get total number of history entries
int get_history_count(void) {
    return history_count;
}

// Get all history entries (for history command)
int get_all_history(const char **history, int max_entries) {
    int count = (history_count < max_entries) ? history_count : max_entries;
    
    for (int i = 0; i < count; i++) {
        int pos = (history_next - history_count + i + MAX_HISTORY) % MAX_HISTORY;
        history[i] = history_buffer[pos];
    }
    
    return count;
}

// Clear history
void clear_history(void) {
    for (int i = 0; i < MAX_HISTORY; i++) {
        if (history_buffer[i] != NULL) {
            free(history_buffer[i]);
            history_buffer[i] = NULL;
        }
    }
    history_count = 0;
    history_next = 0;
}

