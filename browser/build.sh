#!/usr/bin/env bash
set -euo pipefail

version=4.0.23
if ! command -v emcc >/dev/null || ! emcc --version | sed -n '1p' | grep -Fq " $version "; then
    echo "Activate Emscripten SDK $version before building." >&2
    exit 1
fi

cd "$(dirname "${BASH_SOURCE[0]}")/.."
emcc -std=gnu17 -O2 -g0 -I. --no-entry \
    DISK_driver.c DISK_driver_problem1.c DISK_driver_problem2.c DISK_driver_problem3.c \
    browser/mini-fs-engine.c \
    -sENVIRONMENT=web,worker,node -sMODULARIZE=1 -sEXPORT_ES6=1 \
    '-sEXPORTED_RUNTIME_METHODS=["ccall"]' \
    '-sEXPORTED_FUNCTIONS=["_mini_fs_format","_mini_fs_open","_mini_fs_write","_mini_fs_close","_mini_fs_seek","_mini_fs_read","_mini_fs_read_output","_mini_fs_remount","_mini_fs_snapshot"]' \
    -o browser/mini-fs-core.mjs
