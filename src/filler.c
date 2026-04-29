#define _GNU_SOURCE

#include <stdint.h>

/*
This program just contains some branches, which are meant to fill the LBR with some entries.
*/

int main(void)
{
    volatile uint64_t sink = 0;
    uint64_t iteration = 0;

    for (;;) {
        if ((iteration & 1U) == 0)
            sink += iteration;
        else
            sink ^= iteration;

        if ((iteration & 2U) == 0)
            sink += 3;
        else
            sink += 5;

        iteration++;
    }

    return (int)sink;
}