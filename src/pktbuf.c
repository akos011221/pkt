#include "pktbuf.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t g_pool_lock = PTHREAD_MUTEX_INITIALIZER;

int pktbuf_pool_init(pktbuf_pool_t *p, size_t capacity) {
    if (!p || p->capacity == 0) return -1;

    p->free_list = NULL;
    p->capacity = capacity;
    p->available = 0;

    for (size_t i = 0; i < capacity; i++) {
        pktbuf_t *b = (pktbuf_t *)calloc(1, sizeof(pktbuf_t));
        if (!b) return -1;

        // Insert into free list
        b->next = p->free_list;
        p->free_list = b;
        p->available++;
    }

    return 0;
}

void pktbuf_pool_destroy(pktbuf_pool_t *p) {
    if (!p) return;

    pthread_mutex_lock(&g_pool_lock);
    pktbuf_t *cur = p->free_list;
    while (cur) {
        pktbuf_t *n = cur->next;
        free(cur);
        cur = n;
    }
    p->free_list = NULL;
    p->available = 0;
    p->capacity = 0;
    pthread_mutex_unlock(&g_pool_lock);
}

pktbuf_t *pktbuf_alloc(pktbuf_pool_t *p) {
    if (!p) return NULL;

    pthread_mutex_lock(&g_pool_lock);
    pktbuf_t *b = p->free_list;
    if (b) {
        p->free_list = b->next;
        b->next = NULL;
        p->available--;
    }
    pthread_mutex_unlock(&g_pool_lock);

    return b;
}

void pktbuf_free(pktbuf_pool_t *p, pktbuf_t *b) {
    if (!p || !b) return;

    /*
        Allocated buffer is private to a thread, so it is
        safe to clear it before the lock and is better to
        keep the lock as short as possible.
    */
    b->len = 0;

    pthread_mutex_lock(&g_pool_lock);
    b->next = p->free_list;
    p->free_list = b;
    p->available++;
    pthread_mutex_unlock(&g_pool_lock);
}