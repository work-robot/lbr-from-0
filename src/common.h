#ifndef LBR_COMMON_H
#define LBR_COMMON_H

#include <linux/perf_event.h>

#define LBR_COMM_LEN 16
#define LBR_TARGET_COMM "exercise"
#define LBR_MAX_BRANCH_ENTRIES 32

struct lbr_branch_record {
    __u64 from;
    __u64 to;
    __u64 cycles;
    __u8 mispred;
    __u8 predicted;
    __u8 in_tx;
    __u8 abort;
    __u8 type;
    __u8 reserved[3];
};

struct lbr_event {
    __u64 timestamp_ns;
    __u64 pid_tgid;
    __s32 helper_rc;
    __u32 cpu;
    __u32 branch_count;
    __u32 branch_capacity;
    char comm[LBR_COMM_LEN];
    char syscall[24];
    struct lbr_branch_record branches[LBR_MAX_BRANCH_ENTRIES];
};

#endif