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
 * @brief Read the workload data from the provided file.
 */
size_t read_data(size_t workload_size, FILE *file) {
    size_t count = 0;
    char line[256];
    char cmd_buf[128];

    while (fgets(line, sizeof(line), file) && count < workload_size) {
        // skip empty lines or carriage returns
        if (line[0] == '\n' || line[0] == '\r' || line[0] == ' ') continue;

        workload_item item;
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
 * @brief Comparison function to sort workload items by start time (ts).
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
int compare_processes(const void *a, const void *b) {
    const process *p1 = (const process *)a;
    const process *p2 = (const process *)b;

    if (p1->prio != p2->prio) {
        return p2->prio - p1->prio; // higher priority first
    }
    // tie-breaker: lower PID first
    return (int)p1->pid - (int)p2->pid;
}



/**
 * @brief Main simulation loop from t=ts to t=tf.
 *
 * At each timestep:
 *   1. collect all processes where ts <= t <= tf (eligible)
 *   2. sort eligible by priority desc, pid asc
 *   3. greedy fill: add to run queue while cumulative prio <= ncpus
 *   4. remaining go to pending queue with idle/tf penalty
 *   5. record state in timeline
 *
 * Complexity: O(T * N log N)
 */
void time_loop(size_t workload_size, size_t ts, size_t tf, size_t ncpus, pstate **timeline) {
    process *avail = malloc(workload_size * sizeof(process));
    process *run   = malloc(workload_size * sizeof(process));
    process *pend  = malloc(workload_size * sizeof(process));
    if (!avail || !run || !pend) {
        perror("malloc in time_loop");
        free(avail); free(run); free(pend);
        return;
    }

    for (size_t t = ts; t <= tf; t++) {
        size_t nb_avail = 0;

        // collect processes active at time t
        for (size_t i = 0; i < workload_size; i++) {
            if (workload[i].ts <= t && t <= workload[i].tf) {
                avail[nb_avail].pid  = workload[i].pid;
                avail[nb_avail].prio = workload[i].prio;
                nb_avail++;
            }
        }

        if (nb_avail > 0) {
            qsort(avail, nb_avail, sizeof(process), compare_processes);
        }

        // 3. Greedy Fill & Preemption (Person 3)
        size_t current_cpu_load = 0;
        size_t nb_run = 0, nb_pend = 0;
        
        for (size_t i = 0; i < nb_avail; i++) {
            process p = avail[i];

            if (current_cpu_load + (size_t)p.prio <= ncpus) {
                // Fits in CPU Capacity -> Running Queue
                run[nb_run] = p;
                nb_run++;
                current_cpu_load += (size_t)p.prio;
            } else {
                // Preempted or Capacity Full -> Pending Queue
                pend[nb_pend] = p;
                nb_pend++;
                
                // Penalty: Increment idle and tf (Find original process by PID)
                for (size_t w = 0; w < workload_size; w++) {
                    if ((size_t)workload[w].pid == p.pid) {
                        workload[w].idle++;
                        workload[w].tf++;
                        break;
                    }
                }
            }
        }
        record_timeline(t, MAX_STEPS, timeline, run, nb_run, pend, nb_pend, workload_size);
    }

    free(avail);
    free(run);
    free(pend);
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
