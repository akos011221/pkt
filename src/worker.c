#define _POSIX_C_SOURCE 200809L
#include "worker.h"
#include "log.h"
#include "parser.h"

#include <signal.h>
#include <stdatomic.h>
#include <time.h>

// Global stop flag from main; required for all workers.
extern volatile sig_atomic_t g_stop;

static void *worker_main(void *arg) {
    worker_t *w = (worker_t *)arg;

    while (!g_stop) {
        pktbuf_t *b = (pktbuf_t *)ring_pop(w->rx_ring);
        if (!b) {
            // Ring empty => avoid burning CPU while doing nothing.
            struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000}; // 1ms
            nanosleep(&ts, NULL);
            continue;
        }

        w->pkts_in++;

        // Parse the flow
        flow_key_t key;
        if (parse_flow_key(b->data, b->len, &key) != 0) {
            // Not a valid IPv4/TCP/UDP, drop it.
            w->pkts_dropped++;
            pktbuf_free(w->pool, b);
            continue;
        }
        w->pkts_parsed++;

        // Match a rule
        const rule_t *r = rule_table_match(w->rt, &key);
        if (!r) {
            // No match, drop it.
            w->pkts_dropped++;
            pktbuf_free(w->pool, b);
            continue;
        }
        w->pkts_matched++;

        if (r->action.type == ACT_DROP) {
            w->pkts_dropped++;
            pktbuf_free(w->pool, b);
            continue;
        }

        if (r->action.type == ACT_FWD) {
            // Forward out on TX interface the raw L2 frame (as captured).
            if (tx_send(w->tx, b->data, b->len) != 0) {
                w->pkts_forwarded++;
            } else {
                // TX failed => can be considered dropped.
                w->pkts_dropped++;
            }

            pktbuf_free(w->pool, b);
            continue;
        }

        // Unknown action => drop it.
        w->pkts_dropped++;
        pktbuf_free(w->pool, b);
    }

    return NULL;
}

int worker_start(worker_t *w) {
    if (!w) return -1;
    return pthread_create(&w->thread, NULL, worker_main, w);
}

void worker_join(worker_t *w) {
    if (!w) return;
    pthread_join(w->thread, NULL);
}