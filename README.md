# LBR Snapshot Experiment

*WARNING*: This repo is LLM-generated and only partially reviewed by a human.

This project explores how Intel Last Branch Records (LBR) look across context switches. It uses a libbpf-based BPF program to snapshot the branch stack when the `exercise` process enters a sleep syscall, then prints one JSON object per event from userspace.

The setup is intentionally small: `exercise` runs on one CPU and generates a small amount of branch activity before sleeping, `filler` runs on the same CPU and keeps generating branches, and `lbr_snapshot` enables LBR for that CPU and captures snapshots. The goal is to observe cases where the current task has not yet overwritten all LBR entries after being scheduled.

## Contents

- [src/lbr_snapshot.bpf.c](src/lbr_snapshot.bpf.c): BPF tracepoint programs that snapshot LBR on sleep-related syscalls for `exercise`.
- [src/lbr_snapshot.c](src/lbr_snapshot.c): Userspace loader that enables the CPU-wide perf event, attaches the BPF programs, and prints JSON.
- [src/exercise.c](src/exercise.c): Static test workload that branches briefly and then issues a sleep syscall in a loop.
- [src/filler.c](src/filler.c): Companion workload that runs on the same CPU and fills branch history.
- [src/common.h](src/common.h): Shared event/layout definitions used by BPF and userspace.
- [run_demo.sh](run_demo.sh): Launcher that starts the loader off the target CPU and pins `exercise` and `filler` to the selected CPU.
- [Makefile](Makefile): Build rules for the BPF object, skeleton, and helper binaries.
- [PLAN.md](PLAN.md): Original task requirements and implementation plan.

## Build And Run

Build everything with:

```sh
make
```

Run the experiment as root:

```sh
sudo ./run_demo.sh <cpu> <sleep-ms> [run-seconds]
```

By default, each branch entry in the JSON output includes only `from` and `to`. To also print the other `perf_branch_entry` fields, set `LBR_PRINT_BRANCH_DETAILS=1` when running `lbr_snapshot` or the demo script.