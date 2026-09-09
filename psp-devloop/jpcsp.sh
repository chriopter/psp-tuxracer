#!/usr/bin/env bash

# psp-devloop stage 02: run the same in-game benchmark under JPCSP.

set -Eeuo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/benchmark.sh"

launcher="$devloop_root/02-emulator/jpcsp-launch"

if [[ ! -x "$launcher" ]]; then
  printf '[skip] psp-devloop jpcsp-launch not found at %s\n' "$launcher"
  exit 77
fi

# JPCSP needs roughly five times as long as PPSSPP to reach the checkpoint.
run_benchmark JPCSP 600 "$launcher" "$ms0_dir" "$game_dir/EBOOT.PBP"
