#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "workload.h"

struct workload_item_t {
	int pid;       //< the event id
    int ppid;      //< the event parent id
	size_t ts;     //< start date
	size_t tf;     //< finish date
	size_t idle;   //< total time the process has been idle;
	char *cmd;     //< the binary name
	int prio;      //< process priority
};

/**
 * @brief Read the workload data
 * 
 * @param filename the name of file, can be STDIN
 * @return a pointer on a newly allocated workload_item arry, or null if it fails.
 */
workload_item* read_workload_data(size_t workload_size, FILE *file) {
    size_t count = 0;
    char line[256];  // Buffer for each line in the file
    char cmd[50];    // Buffer for command name

    workload_item* workload = malloc(sizeof(workload_item) * workload_size);
    if (!workload) {
        fprintf(stderr, "* Error alloc mem: %s:%d\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }

    while (fgets(line, sizeof(line), file) && count < workload_size) {
        workload_item item;
        // Parse the line into the workload_item structure
        if (sscanf(line, "%d %d %zu %zu %zu %s %d",
                   &item.pid, &item.ppid, &item.ts, &item.tf,
                   &item.idle, cmd, &item.prio) == 7) {
            item.cmd = strdup(cmd);  // Duplicate the string for command name
            workload[count++] = item;
        } else {
            fprintf(stderr, "Error parsing line: %s\n", line);
            free(workload);
			return false;
        }
	}
	return workload;
}


void free_workload(workload_item *workload, size_t nr)
{
    for (size_t i=0; i<nr; i++)
            free(workload[i].cmd);
    free(workload);
}


int get_prio(workload_item* workload, size_t number) {
    return workload[number].prio;
}

size_t get_process_walltime(workload_item* workload, size_t number) {
    return workload[number].tf - workload[number].ts + 1;
}

size_t get_all_process_walltime(workload_item* workload, size_t n) {
    size_t total = 0;
    for (size_t i=0; i<n; i++) {
        total += get_process_walltime( workload, i);
    } 
    return total;
}

void set_increment_idle_time(workload_item* workload, size_t number) {
    workload[number].idle++;
}

bool is_process_in_timeframe(workload_item* workload, size_t number, size_t t) {
    return (workload[number].ts <= t && t <= workload[number].ts);
}

bool is_process_current(workload_item* workload, size_t number, size_t t) {
    return (workload[number].ts <= t && t <= workload[number].ts + workload[number].idle);
}

