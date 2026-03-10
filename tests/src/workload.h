/**
 * @file workload.h
 * @author S. Genaud
 */

 #ifndef _WORKLOAD_H_
 #define _WORKLOAD_H_
 #include <stdio.h>
 #include <stdbool.h>
 
 typedef struct workload_item_t workload_item;
 
 
/**
 * @brief Read the workload data
 * 
 * @param filename the name of file, can be STDIN
 * @return number of data lines read in the file
 */
workload_item* read_workload_data(size_t workload_size, FILE *file);

/**
 * @brief free the workload array
 */
void free_workload(workload_item *workload, size_t nr);

/**
 * @brief Get the prio object
 * 
 * @param workload 
 * @param number 
 * @return int 
 */
int get_prio(workload_item* workload, size_t number);

/**
 * @brief Walltime of one process
 * 
 * @param workload 
 * @param number the process number (pid)
 * @return number of timesteps the process has to run (tf-ts+1)
 */
size_t get_process_walltime(workload_item* workload, size_t number);

/**
 * @brief Get the sum of walltimes of all processes 0 .. n-1
 * 
 * @param workload 
 * @param n the number of processes
 * @return the sum of walltimes of all processes
 */
size_t get_all_process_walltime(workload_item* workload, size_t n);

void set_increment_idle_time(workload_item* workload, size_t number);

bool is_process_in_timeframe(workload_item* workload, size_t number, size_t t);

bool is_process_current(workload_item* workload, size_t number, size_t t);

 #endif
 
