#!/bin/sh
# Build the payload from source. Needs a tic6x-elf GCC (13.3.0 tested) and binutils on PATH, or set TIC6X to its bin directory.
# usage: scripts/build_payload.sh [outdir]      -> payload.bin, payload.elf, payload.nm in outdir (default: build/)
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$HERE/build}"; mkdir -p "$OUT"
if [ -n "$TIC6X" ]; then T="$TIC6X/"; else T=""; fi
"${T}tic6x-elf-gcc" -O2 -mlittle-endian -march=c674x -ffreestanding -fno-pic -fno-stack-protector -fno-builtin \
  -ffunction-sections -fno-asynchronous-unwind-tables -nostdlib -c "$HERE/src/payload.c" -o "$OUT/payload.o"
"${T}tic6x-elf-ld" -T "$HERE/src/link.ld" -o "$OUT/payload.elf" "$OUT/payload.o"
"${T}tic6x-elf-objcopy" -O binary "$OUT/payload.elf" "$OUT/payload.bin"
"${T}tic6x-elf-nm" "$OUT/payload.elf" > "$OUT/payload.nm"
ls -l "$OUT/payload.bin"; grep -E " T loopstart_" "$OUT/payload.nm"
