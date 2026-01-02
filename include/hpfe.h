#ifndef HPFE_H
#define HPFE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *iface; // interface name
    int verbose;       // 0..2
    int duration_sec;  // 0 = run forever
} hpfe_config_t;

#endif