#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <stdlib.h>

typedef struct process_t process;

// The struct is exposed in header not to have to create
// getters and setters for such a small class, so as to
// be able to use it outside sched.c
struct process_t {
	int prio;         //< priority in range [1..10]
	size_t pid;       //< pid assigned by the scheduler
};
#endif
