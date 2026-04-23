#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "sched.h"
#include "trace.h"
#include "process.h"
#include "cpu.h"

/* Global workload definition */
workload_item* workload = NULL;
enum epoch { before, in, after };  //< to compare a date and a timeframe 
typedef enum epoch epoch;


   /**
    *            0,init
    *         /           \
	*      1,bash          2,bash
	*     /   \   \        /      \ 
	* 3,find   \   \      |       |
	*         4,gcc \     |       |
	*            |   |    |       |
	*	       5,ld  |    |       | 
	*                |    6,ssh   |
	*                |    |       |
	*                |    7,crypt |
	*                |           8,snake
    *               9,cat
    */

   /*
	workload_item workload[] = {
	//  pid ppid  ts  tf idle  cmd     prio
	    {0, -1,    0, 18,  0, "init",  10 },
        {1,  0,    1, 16,  0, "bash",   1 },
        {2,  0,    3, 16,  0, "bash",   1 },
        {3,  1,    4,  6,  0, "find",   2 },
        {4,  1,    7,  9,  0, "gcc",    5 },
		{5,  4,    8,  9,  0, "ld",     4 }, 
		{6,  2,   10, 13,  0, "ssh",    3 },
        {7,  6,   11, 13,  0, "crypt",  5 },
        {8,  2,   14, 16,  0, "snake",  4 },
        {9,  1,   14, 15,  0, "cat",    5 },
	};
	*/
void draw_hbar(char c, size_t width) {
	char bar[width+1];
	memset(bar, c, width);
	bar[width]='\0';
	printf("%s",bar);
}

void chronogram(workload_item* workload, size_t nb_processes, size_t timesteps) {
	// draw timeline
	size_t tick;
	size_t freq=5;
	printf("\t");
	for (tick=0; tick<timesteps; tick++) {
		if (tick%freq==0) printf("|"); else printf(".");
	}
	printf("\n");
	// draw processes lifetime
	for (size_t i=0; i<nb_processes; i++) {
		printf("%s\t", workload[i].cmd);
		draw_hbar(' ',workload[i].ts);
		draw_hbar('X',workload[i].tf-workload[i].ts);
		printf("\t\t\t (tf=%zu,idle=%zu)\n", workload[i].tf, workload[i].idle);
	}
}


/**
 * @brief count lines in file
 * 
 * @param file assumed to be an open file
 * @return the number of lines in files 
 */
size_t count_lines_in_file(FILE *file) {
    int lines = 0;
    char ch;
    // count newline characters
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') {
            lines++;
        }
    }
    // If the file is not empty and the last line doesn't end with '\n'
    if (ch != '\n' && lines != 0) {
        lines++;
    }
    rewind(file);
    return lines;
}

/**
 * @brief Read the workload data
 * 
 * @param filename the name of file, can be STDIN
 * @return number of data lines read in the file
 */
size_t read_data(size_t workload_size, FILE *file) {
    size_t count = 0;
    char line[256];   // Buffer for each line in the file
    char cmd_buf[128];   // Buffer for command name

    while (fgets(line, sizeof(line), file) && count < workload_size) {
        // skip empty lines or carriage returns
        if (line[0] == '\n' || line[0] == '\r' || line[0] == ' ') continue;

        workload_item item;
        // Parse the line into the workload_item structure
        if (sscanf(line, "%d %d %zu %zu %zu %s %d",
                   &item.pid, &item.ppid, &item.ts, &item.tf,
                   &item.idle, cmd_buf, &item.prio) == 7) {
            
            item.cmd = strdup(cmd_buf);
            if (!item.cmd) {
                perror("strdup error");
                break;
            }
            workload[count++] = item;
        } else {
            fprintf(stderr, "Error parsing workload line: %s", line);
            break; 
        }
    }
    return count;
}

/**
 * @brief Free all memory allocated for the workload.
 */
void free_workload(size_t size) {
	if (!workload) return;
	for (size_t i = 0; i < size; i++) {
		if (workload[i].cmd) {
			free(workload[i].cmd);
		}
	}
	free(workload);
	workload = NULL;
}

/**
 * @brief Simple helper to sort the workload by start time (ts)
 */
int compare_workload(const void *a, const void *b) {
    const workload_item *item_a = (const workload_item *)a;
    const workload_item *item_b = (const workload_item *)b;
    
    if (item_a->ts != item_b->ts) {
        return (int)item_a->ts - (int)item_b->ts;
    }
    // secondary sort: priority (higher first)
    return item_b->prio - item_a->prio;
}

/**
 * @brief Comparison function for the scheduler's internal queues.
 * Sorts by priority (descending) and PID (ascending) as tie-breaker.
 */
#include "heap.h"

static size_t g_sched_seq = 0;
static size_t *remaining_work = NULL;

static void remove_finished_processes(Heap *runningQueue, size_t *cpu_load) {
    size_t i = 0;
    while (i < heap_size(runningQueue)) {
        workload_item *proc = runningQueue->arr[i];
        if (remaining_work[proc->pid] == 0) {
            *cpu_load -= (size_t)proc->prio;
            heap_remove_at(runningQueue, i);
        } else {
            i++;
        }
    }
}

static void add_arrived_to_pending(Heap *pendingQueue, size_t workload_size, size_t t) {
    for (size_t i = 0; i < workload_size; i++) {
        if (workload[i].ts == t) {
            workload[i].seq = g_sched_seq++;
            heap_insert(pendingQueue, &workload[i]);
        }
    }
}

static void increment_idle_counters(Heap *pendingQueue) {
    for (size_t i = 0; i < heap_size(pendingQueue); i++) {
        workload_item *proc = pendingQueue->arr[i];
        proc->idle++;
    }
}

static void record_state(size_t t, size_t workload_size, pstate **timeline, Heap *runningQueue, Heap *pendingQueue) {
    process *run = malloc(sizeof(process) * (heap_size(runningQueue) + 1));
    process *pend = malloc(sizeof(process) * (heap_size(pendingQueue) + workload_size + 1));
    
    size_t nb_run = 0;
    size_t nb_pend = 0;

    for (size_t i = 0; i < heap_size(runningQueue); i++) {
        workload_item *p = runningQueue->arr[i];
        run[nb_run].pid = (size_t)p->pid;
        run[nb_run].prio = p->prio;
        nb_run++;
    }

    for (size_t i = 0; i < heap_size(pendingQueue); i++) {
        workload_item *p = pendingQueue->arr[i];
        pend[nb_pend].pid = (size_t)p->pid;
        pend[nb_pend].prio = p->prio;
        nb_pend++;
    }

    for (size_t i = 0; i < workload_size; i++) {
        if (workload[i].ts > t) {
            pend[nb_pend].pid = (size_t)workload[i].pid;
            pend[nb_pend].prio = workload[i].prio;
            nb_pend++;
        }
    }

    // Mark anyone who's finished their work as inactive ('_')
    for (size_t i = 0; i < workload_size; i++) {
        if (remaining_work[i] == 0 && workload[i].ts <= t) {
            timeline[i][t] = inactive;
        }
    }

    record_timeline(t, MAX_STEPS, timeline, run, nb_run, pend, nb_pend, workload_size);
    
    free(run);
    free(pend);
}

/**
 * @brief main loop for simulation: describe actions taken at each
 * time step from time ts to tf. 
 */
void time_loop(size_t workload_size, size_t ts, size_t tf, size_t ncpus, pstate **timeline) {
    Heap runningQueue;
    Heap pendingQueue;
    heap_init(&runningQueue, workload_size, min_cmp);
    heap_init(&pendingQueue, workload_size, max_cmp);
    
    size_t cpu_load = 0;
    g_sched_seq = 0;
    remaining_work = malloc(sizeof(size_t) * workload_size);
    for (size_t i = 0; i < workload_size; i++) {
        remaining_work[i] = workload[i].tf - workload[i].ts + 1;
    }

    // Initialize timeline
    for (size_t i = 0; i < workload_size; i++) {
        for (size_t t = 0; t < MAX_STEPS; t++) {
            timeline[i][t] = pending;
        }
    }

    for (size_t t = ts; t < MAX_STEPS; t++) {
        // First, kick out any processes that are done so we have room
        remove_finished_processes(&runningQueue, &cpu_load);

        // See if any new processes are arriving at this tick and put them in the waiting line
        add_arrived_to_pending(&pendingQueue, workload_size, t);

        workload_item **holdQueue = malloc(sizeof(workload_item *) * workload_size);
        size_t holdCount = 0;

        // Main scheduling part: try to move processes from waiting to running
        while (!heap_empty(&pendingQueue)) {
            workload_item *candidate = heap_top(&pendingQueue);
            
            if (cpu_load + (size_t)candidate->prio <= ncpus) {
                // If it fits, just schedule it directly
                heap_pop(&pendingQueue);
                heap_insert(&runningQueue, candidate);
                cpu_load += (size_t)candidate->prio;
            } else {
                // CPU is full, so see if we can preempt the weakest process currently running
                workload_item *lowest = heap_top(&runningQueue);
                if (lowest != NULL && candidate->prio > lowest->prio) {
                    // It has higher priority, so it takes the spot of the weakest runner
                    heap_pop(&pendingQueue);
                    heap_insert(&runningQueue, candidate);
                    cpu_load += (size_t)candidate->prio;

                    // We might need to kick out more processes if the new one is too heavy
                    while (cpu_load > ncpus && !heap_empty(&runningQueue)) {
                        workload_item *evict = heap_top(&runningQueue);
                        heap_pop(&runningQueue);
                        cpu_load -= (size_t)evict->prio;
                        holdQueue[holdCount++] = evict;
                    }
                } else {
                    // Not enough priority to preempt, so it stays in the waiting line
                    heap_pop(&pendingQueue);
                    holdQueue[holdCount++] = candidate;
                }
            }
        }

        // Put everyone who didn't make the cut back into the pending queue
        for (size_t i = 0; i < holdCount; i++) {
            holdQueue[i]->seq = g_sched_seq++;
            heap_insert(&pendingQueue, holdQueue[i]);
        }
        free(holdQueue);

        // Reduce the work remaining for everyone currently using the CPU
        for (size_t i = 0; i < heap_size(&runningQueue); i++) {
            remaining_work[runningQueue.arr[i]->pid]--;
        }

        // Everyone still waiting gets another 'idle' tick added to their stats
        increment_idle_counters(&pendingQueue);

        // Finally, save what's happening right now so we can draw the timeline later
        record_state(t, workload_size, timeline, &runningQueue, &pendingQueue);
    }

    // Update workload tf for final chronogram
    for (size_t i = 0; i < workload_size; i++) {
        // tf is ts + work + idle - 1
        workload[i].tf = workload[i].ts + (workload[i].tf - workload[i].ts + 1) + workload[i].idle - 1;
    }

    heap_free(&runningQueue);
    heap_free(&pendingQueue);
    free(remaining_work);
}

/**
 * main
 */
int main(int argc, char** argv) {
	FILE *input;
	if (argc > 1) { // if one arg, use it to read in data
		if ((input = fopen(argv[1],"r")) == NULL) {
			perror("Error reading file:");
			exit(EXIT_FAILURE);
		}
		else
			printf("* Read from %s ...", argv[1]);
	}
	else { // no arg provided, read from stdin
			printf("* Read from stdin ...");
			input = stdin;
	}
	// read from standard input
	fflush(stdout);
	size_t nr = count_lines_in_file(input);

	workload = malloc(sizeof(workload_item) * nr);
	if (!workload) {
		perror("malloc workload");
		return EXIT_FAILURE;
	}

	size_t workload_size = read_data(nr, input);
	if (workload_size < nr) {
		printf("* Warning: Only %zu/%zu lines loaded successfully.\n", workload_size, nr);
	} else {
		printf("* Loaded %zu lines of data.\n", workload_size);
	}

	// do not sort workload: pid == array index must hold for correct timeline output

	pstate **timeline = alloc_timeline(MAX_STEPS, workload_size);

	if (workload_size > 0) {
		time_loop(workload_size, 0, MAX_STEPS - 1, MAX_CPU_CAP, timeline);
	} else {
		fprintf(stderr, "Error: No valid workload to simulate.\n");
		free_workload(nr);
		if (input != stdin) fclose(input);
		return EXIT_FAILURE;
	}


	printf("* Chronogram === \n");
	chronogram(workload, workload_size, MAX_STEPS - 1);
	print_timeline(MAX_STEPS - 1, workload_size, timeline);
	
	// Cleanup
	free_workload(workload_size);
	free_timeline(workload_size, timeline);
	if (input != stdin) fclose(input);
	
	return 0;
}
