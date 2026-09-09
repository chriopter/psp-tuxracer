#!/usr/bin/env bash

# psp-devloop stage 02: run the same in-game benchmark under JPCSP.
# Exits 77 when the harness or JPCSP itself is unavailable.

set -Eeuo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
devloop_root="${PSP_DEVLOOP_ROOT:-$project_dir/../psp-devloop}"
launcher="$devloop_root/02-emulator/jpcsp-launch"
ms0_dir="$project_dir/state/extremetuxracer/config/ppsspp/PSP"
game_dir="$ms0_dir/GAME/ExtremeTuxRacer"
benchmark_file="$game_dir/config/benchmark"
result_file="$game_dir/config/benchmark-result.json"
emulator_pid=

if [[ ! -x "$launcher" ]]; then
  printf '[skip] psp-devloop jpcsp-launch not found at %s\n' "$launcher"
  exit 77
fi

cleanup() {
  rm -f "$benchmark_file"
  if [[ -n "$emulator_pid" ]] && kill -0 "$emulator_pid" 2>/dev/null; then
    kill "$emulator_pid" 2>/dev/null || true
    wait "$emulator_pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

# The PPSSPP stage normally stages first; stage here too so this runner
# also works on its own.
if [[ ! -f "$game_dir/EBOOT.PBP" ]]; then
  python3 "$project_dir/tools/stage-extremetuxracer.py"
fi

mkdir -p "$game_dir/config"
rm -f "$result_file"
printf '600 frozen_river\n' >"$benchmark_file"

# JPCSP needs roughly five times as long as PPSSPP to reach the checkpoint.
timeout --signal=TERM 600 "$launcher" "$ms0_dir" "$game_dir/EBOOT.PBP" &
emulator_pid=$!

for ((attempt = 0; attempt < 600; ++attempt)); do
  if [[ -s "$result_file" ]]; then
    python3 -m json.tool "$result_file" >/dev/null
    printf 'PASS: JPCSP produced %s\n' "$result_file"
    exit 0
  fi

  if ! kill -0 "$emulator_pid" 2>/dev/null; then
    status=0
    wait "$emulator_pid" || status=$?
    if (( status == 77 )); then
      exit 77
    fi
    printf 'FAIL: JPCSP exited without a benchmark result\n' >&2
    exit 1
  fi

  sleep 1
done

printf 'FAIL: JPCSP did not reach the benchmark checkpoint\n' >&2
exit 124
