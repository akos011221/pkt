#include "ring.h"
#include <stdlib.h>

/*
    x = 8    = 1000
    x-1 = 7  = 0111
    x & (x-1) = 1000 & 0111 = 0000
*/
static bool is_pow2(size_t x) {
    return x != 0 && (x & (x - 1) == 0);
}

int ring_init(spsc_ring_t *r, size_t capacity) {
    if (!r || !is_pow2(capacity)) return -1;

    r->slots = (void **)calloc(capacity, sizeof(void *));
    if (!r->slots) return -1;

    r->capacity = capacity;
    r->mask = capacity - 1;

    atomic_store(&r->head, 0);
    atomic_store(&r->tail, 0);
    return 0;
}

void ring_destroy(spsc_ring_t *r) {
    if (!r) return;
    free(r->slots);
    r->slots = NULL;
    r->capacity = 0;
    r->mask = 0;
}

/*
    Producer: Needs to see freed slots (space availability) and publish
    data writes.
*/
bool ring_push(spsc_ring_t *r, void *ptr) {
    // "head" is local => relaxed, "tail" is consumer managed => acquire
    size_t head = atomic_load_explicit(&r->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);

    size_t next_head = head + 1;
    if (next_head - tail > r->capacity) {
        // Ring is full.
        return false;
    }

    r->slots[head & r->mask] = ptr;

    // Release store here ensures that the pointer write to slots is visible
    // before advancing head => consumer will see correct pointer.
    atomic_store_explicit(&r->head, next_head, memory_order_release);
    return true;
}

/*
    Consumer: Needs to see slot data (content) and publish read completion
    (slot freeing).
*/
void *ring_pop(spsc_ring_t *r) {
    // "tail" is local => relaxed, "head" is producer managed => acquire
    size_t tail = atomic_load_explicit(&r->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&r->head, memory_order_acquire);

    if (tail == head) {
        // Consumer caught up to producer => no items to read.
        return NULL;
    }

    void *ptr = r->slots[tail & r->mask];

    // Release store here ensures that slot read completion is visible before
    // telling that the slot is free.
    atomic_store_explicit(&r->tail, tail + 1, memory_order_release);
    return ptr;
}