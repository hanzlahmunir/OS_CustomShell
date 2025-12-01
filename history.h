#ifndef HISTORY_H
#define HISTORY_H

// Initialize history
void init_history(void);

// Add command to history
void add_to_history(const char *command);

// Get history entry by index (1-based, like bash)
// Returns NULL if index is out of range
const char *get_history_entry(int index);

// Get total number of history entries
int get_history_count(void);

// Get all history entries (for history command)
// Returns number of entries, fills history array
int get_all_history(const char **history, int max_entries);

// Clear history
void clear_history(void);

#endif // HISTORY_H

