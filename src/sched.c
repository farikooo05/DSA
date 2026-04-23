#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "sched.h"
#include "trace.h"
#include "process.h"
#include "cpu.h"

#define END_STEP 30

struct workload_item_t {
	int pid;       //< the event id
    int ppid;      //< the event parent id
	size_t ts;     //< start date
	size_t tf;     //< finish date
	size_t idle;   //< total time the process has been idle;
	char *cmd;     //< the binary name
	int prio;      //< process priority
};

workload_item *workload = NULL;
size_t workload_count = 0;

typedef struct {
    int pid;
    int prio;
    size_t ts;
    size_t tf;
    size_t idle;
    size_t seq;
    int state; /* 0=not_arrived 1=pending 2=running 3=finished */
} proc_state;

static proc_state *g_pool = NULL;
static size_t g_seq = 0;

/* Sort pending: Prio DESC, Seq ASC (FIFO) — matches max_cmp in reference */
int cmp_pend(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    if (g_pool[ia].prio != g_pool[ib].prio)
        return g_pool[ib].prio - g_pool[ia].prio;
    if (g_pool[ia].seq != g_pool[ib].seq)
        return (g_pool[ia].seq < g_pool[ib].seq) ? -1 : 1;
    return 0;
}

/* Weakest runner: Prio ASC, then PID DESC — matches min_cmp in reference */
int find_weakest_runner(proc_state *pool, size_t n) {
    int idx = -1;
    for (size_t i = 0; i < n; i++) {
        if (pool[i].state != 2) continue;
        if (idx == -1) { idx = (int)i; continue; }
        if (pool[i].prio < pool[idx].prio ||
           (pool[i].prio == pool[idx].prio && pool[i].pid > pool[idx].pid)) {
            idx = (int)i;
        }
    }
    return idx;
}

void chronogram(workload_item *wl, size_t nb, size_t timesteps) {
	printf("\t");
	for (size_t tick = 0; tick < timesteps; tick++) {
		if (tick % 5 == 0) printf("|"); else printf(".");
	}
	printf("\n");
	for (size_t i = 0; i < nb; i++) {
		printf("%s\t", wl[i].cmd);
		for (size_t t = 0; t < wl[i].ts; t++) printf(" ");
		for (size_t t = wl[i].ts; t < wl[i].tf; t++) printf("X");
		printf("\t\t\t (tf=%zu,idle=%zu)\n", wl[i].tf, wl[i].idle);
	}
}

size_t count_lines_in_file(FILE *file) {
    int lines = 0;
    char ch;
    while ((ch = fgetc(file)) != EOF) if (ch == '\n') lines++;
    if (ch != '\n' && lines != 0) lines++;
    rewind(file);
    return lines;
}

size_t read_data(size_t workload_size, FILE *file) {
    size_t count = 0;
    char line[256], cmd[50];
    while (fgets(line, sizeof(line), file) && count < workload_size) {
        workload_item item;
        if (sscanf(line, "%d %d %zu %zu %zu %s %d", &item.pid, &item.ppid, &item.ts, &item.tf, &item.idle, cmd, &item.prio) == 7) {
            item.cmd = strdup(cmd);
            workload[count++] = item;
        }
	}
	return count;
}

void time_loop(size_t workload_size, size_t ts, size_t tf, size_t capacity, pstate **timeline) {
    proc_state *pool = calloc(workload_size, sizeof(proc_state));
    for (size_t i = 0; i < workload_size; i++) {
        pool[i].pid = workload[i].pid; pool[i].prio = workload[i].prio;
        pool[i].ts = workload[i].ts; pool[i].tf = workload[i].tf;
    }
    g_pool = pool;

    int *pend_idx = malloc(sizeof(int) * workload_size);
    int *hold_list = malloc(sizeof(int) * workload_size * 2);

    for (size_t t = ts; t <= tf; t++) {
        int load = 0;

        /* Step 1: evict finished, compute current load */
        for (size_t i = 0; i < workload_size; i++) {
            if (pool[i].state == 2) {
                if (pool[i].tf < t) pool[i].state = 3;
                else load += pool[i].prio;
            }
        }

        /* Step 2: enqueue newly arrived into pending */
        for (size_t i = 0; i < workload_size; i++) {
            if (pool[i].state == 0 && pool[i].ts == t) {
                pool[i].state = 1;
                pool[i].seq = g_seq++;
            }
        }

        /* Step 3: promote pending -> running (one-pass with preemption) */
        size_t np = 0;
        for (size_t i = 0; i < workload_size; i++)
            if (pool[i].state == 1) pend_idx[np++] = (int)i;
        if (np > 0) qsort(pend_idx, np, sizeof(int), cmp_pend);

        size_t holdCount = 0;

        for (size_t i = 0; i < np; i++) {
            int ci = pend_idx[i];

            if (load + pool[ci].prio <= (int)capacity) {
                /* fits directly */
                pool[ci].state = 2;
                load += pool[ci].prio;
            } else {
                int w = find_weakest_runner(pool, workload_size);
                if (w != -1 && pool[ci].prio > pool[w].prio) {
                    /* preempt: schedule candidate, then evict until under cap */
                    pool[ci].state = 2;
                    load += pool[ci].prio;
                    while (load > (int)capacity) {
                        int victim = find_weakest_runner(pool, workload_size);
                        if (victim == -1) break;
                        pool[victim].state = 4; /* hold */
                        load -= pool[victim].prio;
                        pool[victim].tf++;
                        hold_list[holdCount++] = victim;
                    }
                } else {
                    /* can't fit, can't preempt — hold */
                    pool[ci].state = 4; /* hold */
                    pool[ci].tf++;
                    hold_list[holdCount++] = ci;
                }
            }
        }

        /* re-insert held processes as pending with fresh seq */
        for (size_t i = 0; i < holdCount; i++) {
            pool[hold_list[i]].state = 1;
            pool[hold_list[i]].seq = g_seq++;
        }

        /* Step 4: increment idle for everything still pending */
        for (size_t i = 0; i < workload_size; i++) {
            if (pool[i].state == 1) pool[i].idle++;
        }

        /* Step 5: record timeline */
        process *ra = malloc(sizeof(process) * workload_size);
        process *pa = malloc(sizeof(process) * workload_size);
        size_t nr = 0, npa = 0;
        for (size_t i = 0; i < workload_size; i++) {
            if (pool[i].state == 2) {
                ra[nr].pid = pool[i].pid; ra[nr].prio = pool[i].prio; nr++;
            } else if (pool[i].state == 1 || (pool[i].state == 0 && pool[i].ts > t)) {
                pa[npa].pid = pool[i].pid; pa[npa].prio = pool[i].prio; npa++;
            }
        }
        record_timeline(t, tf + 1, timeline, ra, nr, pa, npa, workload_size);
        free(ra); free(pa);
    }

    for (size_t i = 0; i < workload_size; i++) {
        workload[i].tf = pool[i].tf;
        workload[i].idle = pool[i].idle;
    }
    free(pend_idx); free(hold_list); free(pool);
}

int main(int argc, char **argv) {
    FILE *in = (argc > 1) ? fopen(argv[1], "r") : stdin;
    if (!in) exit(1);
    size_t nr = count_lines_in_file(in);
    workload = malloc(sizeof(workload_item) * nr);
    workload_count = read_data(nr, in);
    pstate **tl = alloc_timeline(END_STEP, workload_count);
    if (workload_count > 0) time_loop(workload_count, 0, END_STEP - 1, CPU_CAPABILITY, tl);
    printf("* Chronogram === \n");
    chronogram(workload, workload_count, END_STEP - 1);
    print_timeline(END_STEP - 1, workload_count, tl);
    for (size_t i = 0; i < workload_count; i++) free(workload[i].cmd);
    free(workload); free_timeline(workload_count, tl);
    return 0;
}
