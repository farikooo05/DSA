#ifndef HEAP_H
#define HEAP_H

#include <stdlib.h>
#include <stdbool.h>
#include "sched.h"

/* cmp(a, b) returns true if a should be above b */
typedef bool (*heap_cmp)(const workload_item *a, const workload_item *b);

typedef struct {
    workload_item **arr;
    size_t size;
    size_t capacity;
    heap_cmp cmp;
} Heap;

bool min_cmp(const workload_item *a, const workload_item *b);
bool max_cmp(const workload_item *a, const workload_item *b);

void heap_init(Heap *h, size_t capacity, heap_cmp cmp);
bool heap_empty(Heap *h);
size_t heap_size(Heap *h);
workload_item *heap_top(Heap *h);
void heap_insert(Heap *h, workload_item *proc);
workload_item *heap_pop(Heap *h);
void heap_remove_at(Heap *h, size_t idx);
void heap_free(Heap *h);

#endif
