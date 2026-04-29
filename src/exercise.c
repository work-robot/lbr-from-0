#define _GNU_SOURCE

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
This program generates a couple entries in the LBR (fewer than LBR_MAX_BRANCH_ENTRIES) and then execute a syscall that
triggers a context switch.
More specifically, we wait using nanosleep(), for the duration specified in ms as the command-line argument.
*/

static void abort_errno(const char *what)
{
    fprintf(stderr, "%s: %s\n", what, strerror(errno));
    abort();
}

static void abort_msg(const char *what)
{
    fprintf(stderr, "%s\n", what);
    abort();
}

static void branchy_prelude(uint64_t iteration, volatile uint64_t *sink)
{
    if ((iteration & 1U) == 0)
        *sink += iteration;
    else
        *sink ^= iteration;

    if ((iteration % 3U) == 0)
        *sink += 7;
    else
        *sink += 11;
}

int main(int argc, char **argv)
{
    struct timespec req;
    volatile uint64_t sink = 0;
    uint64_t iteration = 0;
    long sleep_ms;

    if (argc != 2)
        abort_msg("usage: exercise <sleep-ms>");

    errno = 0;
    sleep_ms = strtol(argv[1], NULL, 10);
    if (errno != 0 || sleep_ms < 0)
        abort_msg("invalid sleep duration");

    req.tv_sec = sleep_ms / 1000;
    req.tv_nsec = (sleep_ms % 1000) * 1000000L;

    for (;;) {
        branchy_prelude(iteration, &sink);
        if (nanosleep(&req, NULL) != 0)
            abort_errno("nanosleep");
        iteration++;
    }

    return (int)sink;
}