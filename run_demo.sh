#!/usr/bin/env bash

# run_demo.sh <cpu> <sleep-ms> [run-seconds]
# The <cpu> specifies the CPU on which to run the exercise and filler programs (LBR will be enabled here).
# The <sleep-ms> specifies how long the exercise program should sleep for in each iteration (in milliseconds).
# The optional [run-seconds] specifies how long the demo should run before exiting (default: 10 seconds).

set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "run_demo.sh must be run as root" >&2
    exit 1
fi

if [[ $# -lt 2 || $# -gt 3 ]]; then
    echo "usage: $0 <cpu> <sleep-ms> [run-seconds]" >&2
    exit 1
fi

CPU="$1"
SLEEP_MS="$2"
RUN_SECONDS="${3:-10}"
ONLINE_CPUS="$(nproc --all)"

if ! [[ "${CPU}" =~ ^[0-9]+$ ]]; then
    echo "cpu must be a non-negative integer" >&2
    exit 1
fi

if (( CPU < 0 || CPU >= ONLINE_CPUS )); then
    echo "cpu ${CPU} is out of range for ${ONLINE_CPUS} online CPUs" >&2
    exit 1
fi

SNAPSHOT_CPU_LIST=""
for ((index = 0; index < ONLINE_CPUS; index++)); do
    if (( index == CPU )); then
        continue
    fi
    if [[ -n "${SNAPSHOT_CPU_LIST}" ]]; then
        SNAPSHOT_CPU_LIST+=","
    fi
    SNAPSHOT_CPU_LIST+="${index}"
done

if [[ -z "${SNAPSHOT_CPU_LIST}" ]]; then
    echo "need at least two CPUs so lbr_snapshot can avoid cpu ${CPU}" >&2
    exit 1
fi

cleanup() {
    if [[ -n "${EXERCISE_PID:-}" ]]; then
        kill "${EXERCISE_PID}" 2>/dev/null || true
    fi
    if [[ -n "${FILLER_PID:-}" ]]; then
        kill "${FILLER_PID}" 2>/dev/null || true
    fi
    if [[ -n "${SNAPSHOT_PID:-}" ]]; then
        kill "${SNAPSHOT_PID}" 2>/dev/null || true
    fi
}

trap cleanup EXIT INT TERM

taskset -c "${SNAPSHOT_CPU_LIST}" ./lbr_snapshot "${CPU}" &
SNAPSHOT_PID=$!

taskset -c "${CPU}" ./filler &
FILLER_PID=$!

taskset -c "${CPU}" ./exercise "${SLEEP_MS}" &
EXERCISE_PID=$!

sleep "${RUN_SECONDS}"