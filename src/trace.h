#ifndef _TRACE_H_
#define _TRACE_H_
#include <stdlib.h>
#include "process.h"  // for process_t 

enum  pstate { inactive, running, pending };   
typedef enum pstate pstate; 

/**
 * @brief allocate storage for timeline, a 2-d array: pids x timesteps
 * 
 * @param timesteps number of timesteps
 * @param nb_processes number of processes
 * @return a pointer to alloc storage
 */
pstate **alloc_timeline(size_t timesteps, size_t nb_processes);

/**
 * @brief record activity of running and pending processes at a given timestep
 * 
 * @param timestep the timestep
 * @param max_step the maxumum number of timesteps of the simulation
 * @param timeline the structure to record activity to
 * @param run the array of `process` items (prio,pid) in run queue
 * @param nb_run length of run
 * @param pend the array of `process` items (prio,pid) in pending queue
 * @param nb_pend length of pend
 * @param max_processes the total number of processes in simulation
 */
void record_timeline(size_t timestep, size_t max_step, 
                    pstate **timeline, 
                    process *run, size_t nb_run,
                    process *pend, size_t nb_pend,
                    size_t max_processes);

/**
 * @brief print the timeline
 * 
 * @param timesteps number of timesteps
 * @param nb_processes number of processes
 * @param ptl 
 */
void print_timeline(size_t timesteps, size_t nb_procs, pstate** ptl);

/**
 * @brief free the timeline struct
 */
void free_timeline(size_t nb_processes,  pstate **timeline);

#endif
