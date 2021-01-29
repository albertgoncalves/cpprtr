#!/usr/bin/env bash

set -e

if [ ! "$(uname -s)" = "Linux" ]; then
    exit 1
fi

(
    sudo sh -c "echo 1 > /proc/sys/kernel/perf_event_paranoid"
    sudo sh -c "echo 0 > /proc/sys/kernel/kptr_restrict"
    perf record --call-graph fp "$WD/bin/main" "$WD/out/main.bmp"
    perf report
    rm perf.data*
)

if [ -z "$1" ]; then
    exit 0
fi

(
    valgrind \
        --tool=cachegrind \
        --branch-sim=yes \
        "$WD/bin/main" "$WD/out/main.bmp" \
        | less
    for x in cachegrind.out.*; do
        cg_annotate --auto=yes "$x" | less
    done
    rm cachegrind.out.*
)
