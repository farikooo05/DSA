#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "trace.h"

pstate **alloc_timeline(size_t timesteps, size_t nb_processes) {
    pstate** ptl  = malloc(nb_processes * sizeof(pstate *)); // processes x timesteps matrix
    for (size_t p=0; p<nb_processes; p++) {
        ptl[p] = malloc(timesteps*sizeof(pstate));
        for (size_t t=0; t<timesteps; t++ )
            ptl[p][t] = inactive;
    }
    return ptl;
}

void record_timeline(size_t timestep, size_t max_step, 
    pstate **timeline, 
    process *run, size_t nb_run,
    process *pend, size_t nb_pend,
    size_t max_processes) 
    {
    assert(timestep < max_step); // make sure we don't exceed #steps in timesteps
    for (size_t i=0; i< nb_run; i++) {
        int pid  = run[i].pid;
        // make sure pid number is < to #lines in input file
        assert(pid < max_processes);
        timeline[pid][timestep] = running;
    }
    for (size_t i=0; i< nb_pend; i++) {
        int pid  = pend[i].pid;
        // make sure pid number is < to #lines in input file
        assert(pid < max_processes);
        timeline[pid][timestep] = pending;
    }
}

void print_timeline(size_t timesteps, size_t nb_procs, pstate** ptl) {
    printf("\n[===Results===]\n");
    for (size_t p=0; p<nb_procs; p++) {
        printf("%zu\t", p);
        for (size_t t=0; t<timesteps; t++ )
            switch (ptl[p][t]) {
                case inactive: printf("_"); break;
                case running: printf("R"); break;
                case pending: printf("."); break;
            }
        printf("\n");
    }
}

void free_timeline(size_t nb_processes,  pstate **timeline) {
    for (size_t pid=0; pid<nb_processes; pid++) {
        free(timeline[pid]);
    }
   free(timeline);
}
