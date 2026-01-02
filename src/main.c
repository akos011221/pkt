#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hpfe.h"
#include "log.h"

static volatile sig_atomic_t g_stop = 0;

static void handle_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

static void install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;

    sa.sa_flags = SA_RESTART;

    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) != 0) {
        log_msg(LOG_ERROR, "sigaction(sigint) failed: %s", strerror(errno));
        exit(1);
    }
    if (sigaction(SIGTERM, &sa, NULL) != 0) {
        log_msg(LOG_ERROR, "sigaction(sigterm) failed: %s", strerror(errno));
        exit(1);
    }
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s --iface <name> [--verbose <0..2>] [--duration <sec>]\n"
            "\n"
            "  --iface     Network interface name (e.g., eth0)\n"
            "  --verbose   0=warn+error, 1=info (default), 2=debug\n"
            "  --duration  Run time in seconds (0 = forever, default 0)\n",
            prog);
}

static int parse_int(const char *s, int *out) {
    errno = 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);

    if (errno != 0)
        return -1;
    if (end == s || *end != '\0')
        return -1;
    if (v < -2147483648L || v > 2147483647L)
        return -1;

    *out = (int)v;
    return 0;
}

static int parse_args(int argc, char **argv, hpfe_config_t *cfg) {
    cfg->iface = NULL;
    cfg->verbose = 1;
    cfg->duration_sec = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--iface") == 0) {
            if (i + 1 >= argc)
                return -1;
            cfg->iface = argv[++i];
        } else if (strcmp(arg, "--verbose") == 0) {
            if (i + 1 >= argc)
                return -1;
            int v = 0;
            if (parse_int(argv[++i], &v) != 0)
                return -1;
            if (v < 0 || v > 2)
                return -1;
            cfg->verbose = v;
        } else if (strcmp(arg, "--duration") == 0) {
            if (i + 1 >= argc)
                return -1;
            int d = 0;
            if (parse_int(argv[++i], &d) != 0)
                return -1;
            if (d < 0)
                return -1;
            cfg->duration_sec = d;
        } else if (strcmp(arg, "--help") == 0) {
            usage(argv[0]);
            exit(0);
        } else {
            return -1;
        }
    }
    if (cfg->iface == NULL) {
        return -1;
    }
    return 0;
}

static log_level_t verbosity_to_level(int verbose) {
    // verbose: 0..2
    // level: WARN/INFO/DEBUG
    if (verbose <= 0)
        return LOG_WARN;
    if (verbose == 1)
        return LOG_INFO;
    return LOG_DEBUG;
}

int main(int argc, char **argv) {
    hpfe_config_t cfg;

    if (parse_args(argc, argv, &cfg) != 0) {
        usage(argv[0]);
        return 2;
    }

    log_set_level(verbosity_to_level(cfg.verbose));

    log_msg(LOG_INFO, "hpfe started");
    log_msg(LOG_INFO, "iface=%s, verbose=%d, duration=%d", cfg.iface, cfg.verbose,
            cfg.duration_sec);

    install_signal_handlers();

    time_t start = time(NULL);

    while (!g_stop) {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 100 * 1000 * 1000; // 100ms
        nanosleep(&ts, NULL);

        if (cfg.duration_sec > 0) {
            time_t now = time(NULL);
            if ((int)(now - start) >= cfg.duration_sec) {
                log_msg(LOG_INFO, "duration reached, stopping");
                break;
            }
        }
    }

    log_msg(LOG_INFO, "hpfe shutting down");
    return 0;
}