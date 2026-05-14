#!/usr/bin/env bash

# run_demo.sh <cpu>
# Runs the LBR user-kernel transition experiment on the specified CPU.

set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "run_demo.sh must be run as root" >&2
    exit 1
fi

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <cpu>" >&2
    exit 1
fi

CPU="$1"
ONLINE_CPUS="$(nproc --all)"

if ! [[ "${CPU}" =~ ^[0-9]+$ ]]; then
    echo "cpu must be a non-negative integer" >&2
    exit 1
fi

if (( CPU >= ONLINE_CPUS )); then
    echo "cpu ${CPU} is out of range for ${ONLINE_CPUS} online CPUs" >&2
    exit 1
fi

taskset -c "${CPU}" ./lbr_snapshot "${CPU}"
