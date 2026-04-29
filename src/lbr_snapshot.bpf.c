#include <linux/bpf.h>
#include <linux/perf_event.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "common.h"

/*
This BPF program takes a snapshot of the LBR when a nanosleep or clock_nanosleep syscall is entered, and submits it to
userspace via a ring buffer.
*/

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 20);
} events SEC(".maps");

struct lbr_scratch {
    struct perf_branch_entry entries[LBR_MAX_BRANCH_ENTRIES];
};

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, struct lbr_scratch);
} scratch SEC(".maps");

static __always_inline int comm_matches_target(void)
{
    char comm[LBR_COMM_LEN];

    if (bpf_get_current_comm(comm, sizeof(comm)) != 0)
        return 0;

    return comm[0] == 'e' && comm[1] == 'x' && comm[2] == 'e' && comm[3] == 'r' &&
           comm[4] == 'c' && comm[5] == 'i' && comm[6] == 's' && comm[7] == 'e' &&
           comm[8] == '\0';
}

static __always_inline void fill_event_metadata(struct lbr_event *event, int is_clock_nanosleep)
{
    event->timestamp_ns = bpf_ktime_get_ns();
    event->pid_tgid = bpf_get_current_pid_tgid();
    event->cpu = bpf_get_smp_processor_id();
    event->helper_rc = 0;
    event->branch_count = 0;
    event->branch_capacity = LBR_MAX_BRANCH_ENTRIES;
    bpf_get_current_comm(event->comm, sizeof(event->comm));
    if (is_clock_nanosleep) {
        event->syscall[0] = 'c';
        event->syscall[1] = 'l';
        event->syscall[2] = 'o';
        event->syscall[3] = 'c';
        event->syscall[4] = 'k';
        event->syscall[5] = '_';
        event->syscall[6] = 'n';
        event->syscall[7] = 'a';
        event->syscall[8] = 'n';
        event->syscall[9] = 'o';
        event->syscall[10] = 's';
        event->syscall[11] = 'l';
        event->syscall[12] = 'e';
        event->syscall[13] = 'e';
        event->syscall[14] = 'p';
        event->syscall[15] = '\0';
    } else {
        event->syscall[0] = 'n';
        event->syscall[1] = 'a';
        event->syscall[2] = 'n';
        event->syscall[3] = 'o';
        event->syscall[4] = 's';
        event->syscall[5] = 'l';
        event->syscall[6] = 'e';
        event->syscall[7] = 'e';
        event->syscall[8] = 'p';
        event->syscall[9] = '\0';
    }
}

static __always_inline int handle_sleep_syscall(int is_clock_nanosleep)
{
    __u32 key = 0;
    struct lbr_scratch *scratch_buffer;
    struct lbr_event *event;
    int bytes_written;
    __u32 count;
    int index;

    scratch_buffer = bpf_map_lookup_elem(&scratch, &key);
    if (!scratch_buffer)
        return 0;

    bytes_written = bpf_get_branch_snapshot(scratch_buffer->entries, sizeof(scratch_buffer->entries), 0);
    if (!comm_matches_target())
        return 0;

    event = bpf_ringbuf_reserve(&events, sizeof(*event), 0);
    if (!event)
        return 0;

    fill_event_metadata(event, is_clock_nanosleep);
    event->helper_rc = bytes_written;
    if (bytes_written > 0) {
        count = (__u32)bytes_written / sizeof(struct perf_branch_entry);
        if (count > LBR_MAX_BRANCH_ENTRIES)
            count = LBR_MAX_BRANCH_ENTRIES;
        event->branch_count = count;

        for (index = 0; index < LBR_MAX_BRANCH_ENTRIES; index++) {
            if (index >= count)
                break;
            event->branches[index].from = scratch_buffer->entries[index].from;
            event->branches[index].to = scratch_buffer->entries[index].to;
            event->branches[index].mispred = scratch_buffer->entries[index].mispred;
            event->branches[index].predicted = scratch_buffer->entries[index].predicted;
            event->branches[index].in_tx = scratch_buffer->entries[index].in_tx;
            event->branches[index].abort = scratch_buffer->entries[index].abort;
            event->branches[index].cycles = scratch_buffer->entries[index].cycles;
            event->branches[index].type = scratch_buffer->entries[index].type;
        }
    }

    bpf_ringbuf_submit(event, 0);
    return 0;
}

SEC("tracepoint/syscalls/sys_enter_nanosleep")
int handle_sys_enter_nanosleep(void *ctx)
{
    (void)ctx;
    return handle_sleep_syscall(0);
}

SEC("tracepoint/syscalls/sys_enter_clock_nanosleep")
int handle_sys_enter_clock_nanosleep(void *ctx)
{
    (void)ctx;
    return handle_sleep_syscall(1);
}

char LICENSE[] SEC("license") = "GPL";