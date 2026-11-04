#!/usr/bin/env sh
set -eu

root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
scratch=$(mktemp -d /tmp/mini-fs-tests.XXXXXX)
case "$scratch" in /tmp/mini-fs-tests.*) ;; *) exit 1 ;; esac
trap 'rm -rf -- "$scratch"' EXIT HUP INT TERM
cd "$scratch"

compiler=${CC:-cc}
sanitizers=''
if [ "${SANITIZE:-0}" = 1 ]; then
    sanitizers='-g -fsanitize=address,undefined -fno-omit-frame-pointer'
    export ASAN_OPTIONS=detect_leaks=1
    export UBSAN_OPTIONS=halt_on_error=1
fi

"$compiler" -std=c11 -Wall -Wextra -Werror -Wno-unknown-pragmas $sanitizers -I "$root" \
    "$root/tests/driver-regression.c" "$root"/DISK_driver*.c "$root/shellmemory.c" -o driver-test
./driver-test

"$compiler" -fcommon $sanitizers "$root"/*.c -o mykernel
mkdir BackingStore
cat > demo.txt <<'EOF'
mount partition4 10 5
open_EXAM 0 file1
write_EXAM 0 [hello world test]
close_EXAM 0

open_EXAM 0 file1
read_EXAM 0 | set x
seek_EXAM 0 -4
read_EXAM 0 var
print var            # -> test
close_EXAM 0
print x              # -> hello world test
quit
EOF
./mykernel < demo.txt > demo.out
grep -Fxq '$ test' demo.out
grep -Fxq '$ hello world test' demo.out
printf '%s\n' 'Mini File System README shell demo passed.'
