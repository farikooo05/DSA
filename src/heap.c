#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "heap.h"

static void swap(workload_item **a, workload_item **b) {
    workload_item *tmp = *a;
    *a = *b;
    *b = tmp;
}

bool min_cmp(const workload_item *a, const workload_item *b) {
    if (a->prio != b->prio)
        return a->prio < b->prio;
    return a->pid > b->pid;
}

bool max_cmp(const workload_item *a, const workload_item *b) {
    if (a->prio != b->prio)
        return a->prio > b->prio;
    return a->seq < b->seq;
}

void heap_init(Heap *h, size_t capacity, heap_cmp cmp) {
    assert(h && cmp);
    h->arr = malloc(sizeof(workload_item *) * capacity);
    if (!h->arr) {
        perror("heap_init: malloc");
        exit(EXIT_FAILURE);
    }
    h->size = 0;
    h->capacity = capacity;
    h->cmp = cmp;
}

bool heap_empty(Heap *h) {
    return h == NULL || h->size == 0;
}

size_t heap_size(Heap *h) {
    return (h == NULL) ? 0 : h->size;
}

workload_item *heap_top(Heap *h) {
    return heap_empty(h) ? NULL : h->arr[0];
}

void heapify_down(Heap *h, size_t idx) {
    while (true) {
        size_t left = 2 * idx + 1;
        size_t right = 2 * idx + 2;
        size_t best = idx;

        if (left < h->size && h->cmp(h->arr[left], h->arr[best]))
            best = left;
        if (right < h->size && h->cmp(h->arr[right], h->arr[best]))
            best = right;

        if (best == idx)
            break;

        swap(&h->arr[idx], &h->arr[best]);
        idx = best;
    }
}

void heapify_up(Heap *h, size_t idx) {
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        if (!h->cmp(h->arr[idx], h->arr[parent]))
            break;
        swap(&h->arr[parent], &h->arr[idx]);
        idx = parent;
    }
}

void heap_insert(Heap *h, workload_item *proc) {
    assert(h && proc);
    if (h->size >= h->capacity) {
        h->capacity = h->capacity ? h->capacity * 2 : 1;
        h->arr = realloc(h->arr, sizeof(workload_item *) * h->capacity);
        if (!h->arr) {
            perror("heap_insert: realloc");
            exit(EXIT_FAILURE);
        }
    }
    h->arr[h->size] = proc;
    h->size++;
    heapify_up(h, h->size - 1);
}

workload_item *heap_pop(Heap *h) {
    if (heap_empty(h))
        return NULL;
    workload_item *top = h->arr[0];
    h->arr[0] = h->arr[h->size - 1];
    h->size--;
    if (h->size > 0)
        heapify_down(h, 0);
    return top;
}

void heap_remove_at(Heap *h, size_t idx) {
    if (h == NULL || idx >= h->size)
        return;
    if (idx == h->size - 1) {
        h->size--;
        return;
    }
    h->arr[idx] = h->arr[h->size - 1];
    h->size--;
    if (idx > 0 && h->cmp(h->arr[idx], h->arr[(idx - 1) / 2]))
        heapify_up(h, idx);
    else
        heapify_down(h, idx);
}

void heap_free(Heap *h) {
    if (h == NULL)
        return;
    free(h->arr);
    h->arr = NULL;
    h->size = 0;
    h->capacity = 0;
}
