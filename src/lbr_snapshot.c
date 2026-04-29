#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <bpf/libbpf.h>

#include "common.h"
#include "lbr_snapshot.skel.h"

/*
lbr_snapshot <cpu>

This program enables LBR on the specified CPU,loads the lbr_snapshot.bpf.c BPF program into the kernel, and listens for
events on the ring buffer. When an event is received, it prints the event as a JSON object in a single line to stdout.

Set the environment variable LBR_PRINT_BRANCH_DETAILS to a non-empty value to also print the additional details for each
branch record.
*/

static volatile sig_atomic_t exiting;
static bool print_branch_details;

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

static long perf_event_open_cpu_lbr(int cpu)
{
    struct perf_event_attr pe;
    int fd;

    memset(&pe, 0, sizeof(pe));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(pe);
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.sample_type = PERF_SAMPLE_BRANCH_STACK;
    pe.branch_sample_type = PERF_SAMPLE_BRANCH_ANY;
    pe.disabled = 1;
    pe.exclude_hv = 1;
    pe.sample_period = ((uint64_t)1) << 62;
    pe.exclude_kernel = 1;

    fd = syscall(__NR_perf_event_open, &pe, -1, cpu, -1, 0);
    if (fd < 0)
        abort_errno("perf_event_open");
    if (ioctl(fd, PERF_EVENT_IOC_RESET, 0) < 0)
        abort_errno("ioctl(PERF_EVENT_IOC_RESET)");
    if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) < 0)
        abort_errno("ioctl(PERF_EVENT_IOC_ENABLE)");

    return fd;
}

static void sig_handler(int signo)
{
    (void)signo;
    exiting = 1;
}

static int handle_event(void *ctx, void *data, size_t data_sz)
{
    const struct lbr_event *event = data;
    uint32_t pid;
    uint32_t tid;
    uint32_t index;

    (void)ctx;
    if (data_sz != sizeof(*event))
        abort_msg("unexpected ring buffer event size");

    pid = (uint32_t)(event->pid_tgid >> 32);
    tid = (uint32_t)event->pid_tgid;

    printf("{");
    printf("\"timestamp_ns\":%llu,", (unsigned long long)event->timestamp_ns);
    printf("\"cpu\":%u,", event->cpu);
    printf("\"pid\":%u,\"tid\":%u,", pid, tid);
    printf("\"comm\":\"%s\",", event->comm);
    printf("\"syscall\":\"%s\",", event->syscall);
    printf("\"helper_rc\":%d,", event->helper_rc);
    printf("\"branch_capacity\":%u,", event->branch_capacity);
    printf("\"branch_count\":%u,", event->branch_count);
    printf("\"branches\":[");
    for (index = 0; index < event->branch_count; index++) {
        const struct lbr_branch_record *record = &event->branches[index];

        if (index != 0)
            printf(",");
        printf("{");
        printf("\"from\":\"0x%llx\",", (unsigned long long)record->from);
        printf("\"to\":\"0x%llx\"", (unsigned long long)record->to);
        if (print_branch_details) {
            printf(",\"mispred\":%u", record->mispred);
            printf(",\"predicted\":%u", record->predicted);
            printf(",\"in_tx\":%u", record->in_tx);
            printf(",\"abort\":%u", record->abort);
            printf(",\"cycles\":%llu", (unsigned long long)record->cycles);
            printf(",\"type\":%u", record->type);
        }
        printf("}");
    }
    printf("]}\n");
    fflush(stdout);

    return 0;
}

static void bump_memlock_rlimit(void)
{
    struct rlimit limit = {
        .rlim_cur = RLIM_INFINITY,
        .rlim_max = RLIM_INFINITY,
    };

    if (setrlimit(RLIMIT_MEMLOCK, &limit) != 0)
        abort_errno("setrlimit(RLIMIT_MEMLOCK)");
}

int main(int argc, char **argv)
{
    struct ring_buffer *ring_buffer = NULL;
    struct lbr_snapshot_bpf *skel = NULL;
    const char *env_value;
    int perf_fd;
    int cpu;
    int err;

    if (argc != 2)
        abort_msg("usage: lbr_snapshot <cpu>");

    errno = 0;
    cpu = (int)strtol(argv[1], NULL, 10);
    if (errno != 0 || cpu < 0)
        abort_msg("invalid cpu");

    env_value = getenv("LBR_PRINT_BRANCH_DETAILS");
    print_branch_details = env_value && env_value[0] != '\0' && strcmp(env_value, "0") != 0;

    bump_memlock_rlimit();
    perf_fd = (int)perf_event_open_cpu_lbr(cpu);

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
    libbpf_set_print(NULL);

    skel = lbr_snapshot_bpf__open_and_load();
    if (!skel)
        abort_msg("lbr_snapshot_bpf__open_and_load failed");

    err = lbr_snapshot_bpf__attach(skel);
    if (err != 0) {
        errno = -err;
        abort_errno("lbr_snapshot_bpf__attach");
    }

    ring_buffer = ring_buffer__new(bpf_map__fd(skel->maps.events), handle_event, NULL, NULL);
    if (!ring_buffer)
        abort_msg("ring_buffer__new failed");

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    while (!exiting) {
        err = ring_buffer__poll(ring_buffer, 250);
        if (err < 0) {
            errno = -err;
            abort_errno("ring_buffer__poll");
        }
    }

    ring_buffer__free(ring_buffer);
    lbr_snapshot_bpf__destroy(skel);
    close(perf_fd);
    return 0;
}