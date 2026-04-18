#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include "workload.h"
#include "read_trace.h"

#define ANSI_COLOR_RED     "\033[31m"
#define ANSI_COLOR_GREEN   "\033[32m"
#define ANSI_COLOR_YELLOW  "\033[33m"
#define ANSI_COLOR_GRAY    "\033[90m"
#define ANSI_COLOR_RESET   "\033[0m"

/** 
 * @brief count lines in file
 *  
 * @param file assumed to be an open file
 * @return the number of lines in files 
 */
size_t count_lines_in_file(FILE *file) {
    int lines = 0; 
    char ch;
    // Count the number of newline characters
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

int compare_size_t_asc(const void *a, const void *b) {
    size_t arg1 = *(const size_t *)a;
    size_t arg2 = *(const size_t *)b;

    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}
int compare_size_t_desc(const void *a, const void *b) {
    size_t arg1 = *(const size_t *)a;
    size_t arg2 = *(const size_t *)b;

    if (arg1 < arg2) return 1;
    if (arg1 > arg2) return -1;
    return 0;
}

void print_queue(char *msg, size_t *q, size_t q_size, workload_item* workload, FILE *out) {
	fprintf(out, "%s |", msg);
	for (size_t i=0;i <q_size;i++) {
		fprintf(out, "%zu (%d) |", q[i], get_prio(workload, q[i]));
	}
	fprintf(out, "\n");
}

int sum_prio(workload_item * workload, const size_t *q, size_t n) {
    size_t sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += get_prio(workload, q[i]);
    }
    return sum;
}

/**
 * @brief Print the load per timesteps given the chronogram parsed trace.
 * 
 * @param timesteps the max number of timesteps
 * @param cum_load the sum of priorities for each timesteps
 */
void print_recap(int* cum_load, size_t timesteps) {
	// --- print recap of cpu load per timestep
	printf("> Recap of CPU load during execution:\n'----------+");
	for (size_t t=0; t<timesteps;t++)
		printf("---");
	printf("`\n| timestep | ");
	for (size_t t=0; t<timesteps;t++) {
		printf("%2zu ",t);
	}
	printf("|\n| load     | ");
	for (size_t t=0; t<timesteps;t++) {
		printf("%2d ", cum_load[t]);
	}
	printf("|\n");
	printf("`----------+");
	for (size_t t=0; t<timesteps;t++)
		printf("---");
	printf("|\n");
}

/**
 * @brief 
 * 
 */
size_t check_priorities_property(char **test_trace, workload_item *workload, 
					size_t nr,  size_t timesteps, size_t cpu_cap) {

	size_t * runq = calloc(nr, sizeof(size_t));
	size_t * waitq = calloc(nr, sizeof(size_t));
	size_t runq_size;  // runq size at each timestep
	size_t waitq_size; // waitq size at each timestep

	assert(runq);
	assert(waitq);
	for (size_t t=0; t<timesteps; t++) {
		runq_size=0;
		waitq_size=0;
		// separate into runq and waitq
		for (size_t i=0; i<nr; i++) {
			if (test_trace[i][t] == 'R') 
				runq[runq_size++] = i;
			else {
				if (is_process_current(workload, i, t))
					waitq[waitq_size++] = i;
			}
		}
		// -- check that no process in waitq has higher priority than those in runq

		// first sort runq asc, waitq desc
		qsort(runq, runq_size, sizeof(size_t), compare_size_t_asc);
		qsort(waitq, waitq_size, sizeof(size_t), compare_size_t_desc);


		// let w_0 be the process with  max priority in waitq 
		// let r_0 be the process with min priority in runq, 
		// if prio(r_0) < prio(w_0) and w_0 can fit in the CPU: that's a violation 
		if (waitq_size>0 && runq_size>0) {
			// first, get the CPU load in the trace
			size_t cpu_load = sum_prio(workload, runq, runq_size); 
			int prio_r_0 = get_prio(workload, runq[0]);
			int prio_w_0 = get_prio(workload, waitq[0]);
			if (prio_r_0 < prio_w_0) {
				// check that this waiting process would fit, instead of the running one.
				if (cpu_load - prio_r_0 + prio_w_0 <= cpu_cap) {
					fprintf(stderr, "** Error: at t=%zu pid=%zu in run queue has lower priority (%d) than pid=%zu in wait queue (%d)\n", 
						t, runq[0], prio_r_0, waitq[0], prio_w_0);
					fprintf(stderr, "\tsize(runq)=%zu size(waitq)=%zu CPU load=%zu\n", runq_size, waitq_size, cpu_load);
					print_queue("\trunq :", runq, runq_size, workload, stderr);
					print_queue("\twaitq:", runq, runq_size, workload, stderr);
					fprintf(stderr, "\tPutting pid=%zu as running and pid=%zu as runing instead, would yield a CPU occupation of %zu\n", 
						waitq[0], runq[0], cpu_load - prio_r_0 + prio_w_0);
					return 16;
				}
			}
		}
	}
	free(runq);
	free(waitq);
	return 0;
}

/**
 * @brief validity
 * 
 * @param test_trace a 2D array representing the schedule trace
 * @param workload representation of the workload input
 * @param nr #linesin workload input = #lines in schedule trace
 * @param cpu_cap max capacity of CPU
 * @param timesteps number of timesteps
 * @return 0: ok
 *         2: exceed CPU capacity at one or more timesteps
 *         4: makespan exceed the given timelimit
 *         8: wrong number of quanta
 * 		  16: violates the property: all running process have 
 *            higher priority than those in waiting queue. 
 *        32: 2 or 4 or 8 or 16
 */ 
int validity(char **test_trace, workload_item *workload, 
					size_t nr, size_t cpu_cap, 
					size_t timesteps, size_t max_duration) {

	int * cum_load=calloc(timesteps, sizeof(int));
	assert(cum_load);

	size_t total_quanta = 0; // total number of quantums seen in trace (=#'R')
	int retcode = 0; // to encode final check results
	size_t duration=0;
	for (size_t t=0; t<timesteps; t++) {
		for (size_t i=0; i<nr; i++) {
			if (test_trace[i][t] == 'R') {
			 	cum_load[t] += get_prio(workload, i); // update the load for this timestep
				total_quanta++; // one more quantum running
			}
			else { // process i is not running 
				// update at the same time the possible idle time for a process:
				// if it not running while in its intial timeframe, increment its idle time.
				if (is_process_current(workload, i, t)) {
					set_increment_idle_time(workload, i);
				}
			}
		}
		if (t>0 && cum_load[t] == 0 && cum_load[t-1]>0) 
			// remember the timestep before when the demand goes down to 0
			duration = t;	
		if  (cum_load[t] > cpu_cap) {
			fprintf(stderr, "* Error: at t=%zu the cumulative load of running processes (%d) overruns CPU capacity (%zu)\n", t, cum_load[t], cpu_cap);
			retcode |= 8; // capacity excess
		}
	}
	// --- print recap of cpu load per timestep
	print_recap(cum_load, timesteps);

	// --- print the makespan of the workload scheduled
	printf("> workload finished at timestep=%zu\n", duration);
	if (duration > max_duration) {
		fprintf(stderr, "* Error: the schedule duration (%zu) exceeds the limit (%zu)\n", duration, max_duration);
		retcode |= 4;
	}
	// -- check the trace has the correct number of quanta ('R')
	size_t expected_quanta = get_all_process_walltime(workload, nr);
	if (total_quanta != expected_quanta) {
		fprintf(stderr, "* Error: wrong number of 'R' in trace: %zu (expected: %zu)\n", total_quanta, expected_quanta);
		retcode |= 8; // number of quantums

	}
	// -- check property on priorities
	retcode |= check_priorities_property(test_trace, workload, nr, timesteps, cpu_cap);

	free(cum_load);
	return retcode;
}


/**
 * main
 */
int main(int argc, char** argv) {
	workload_item* workload;
	FILE *input;

	if (argc < 6) {
		fprintf(stderr, "* Error: usage: %s <workload> <trace> <cpu_cap> <timesteps> <max_duration>\n", argv[0]);
		fprintf(stderr, "     <workload>     = in/test-?.in\n");
		fprintf(stderr, "     <trace>        = out/test-?.out (lines after [===Results===]\n");
		fprintf(stderr, "     <cpu_cap>      = max cpu capacity (int)\n");
		fprintf(stderr, "     <timesteps>    = number of timesteps (int)\n");
		fprintf(stderr, "     <max_duration> = max number of timesteps (int)\n");
		exit(EXIT_FAILURE);
	}
	size_t cpu_cap = atoi(argv[3]);
	size_t timesteps = atoi(argv[4]);
	size_t max_duration = atoi(argv[5]);

	// ------- Read the workload -----
	if ((input = fopen(argv[1],"r")) == NULL) {
			perror("Error reading file:");
			exit(EXIT_FAILURE);
	}
	// read from standard input
	fflush(stdout);
	size_t nr = count_lines_in_file(input);
	// accounting for a newline at the end, this is 1 line less
	nr--;
	printf("* %zu lines in workload data.\n", nr);
	workload = read_workload_data(nr, input);
	fclose(input);
	printf("* Loaded %zu lines of data.\n", nr);

	// ------- Read the trace ---------
    char **test_trace = read_trace(argv[2], nr);
    if (!test_trace) {
        return EXIT_FAILURE;
    }
	// ------- Print parsed trace ------
	#define PRINT_PARSED_TRACE
	#ifdef PRINT_PARSED_TRACE
    printf("* Parsed trace (from %s):\n%s",argv[2], ANSI_COLOR_GRAY);
    for (int i = 0; i < nr; i++) {
        if (test_trace[i])
            printf("%2d: %s\n", i, test_trace[i]);
        else
            printf("%2d: [empty]\n", i);
	}
	printf("%s", ANSI_COLOR_RESET);
	#endif
	// ---- Check
	int retcode = validity(test_trace, workload, nr, cpu_cap, timesteps, max_duration);

	// --- cean and return
    free(test_trace);
	free_workload( workload, nr);
    return retcode;
}

