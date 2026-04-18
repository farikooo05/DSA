#ifndef _SCHED_H_
#define _SCHED_H_

#include <stdlib.h>

/* Simulation Constraints */
#define MAX_STEPS 30      // N = 30 for validation
#define MAX_CPU_CAP 20    // C = 20 capability
#define MAX_THREADS 1     // Single CPU simulation

/**
 * workload_item structure representing a process trace event.
 */
struct workload_item_t {
    int pid;       // the event id (unique)
    int ppid;      // the event parent id
    size_t ts;     // the planned start timestamp
    size_t tf;     // the planned/actual finish timestamp
    size_t idle;   // total time units the process has been idle
    char *cmd;     // the binary/command name
    int prio;      // priority: higher means less chance of interruption
};

typedef struct workload_item_t workload_item;

/* Global workload pointer shared across modules */
extern workload_item* workload;

/**
 * @brief Free all memory allocated for the workload.
 */
void free_workload(size_t size);

/**
 * @brief Comparison function to sort workload items by start time.
 */
int compare_workload(const void *a, const void *b);

/**
 * @brief Comparison function to sort processes by priority (and PID for ties).
 */
int compare_processes(const void *a, const void *b);

#endif
