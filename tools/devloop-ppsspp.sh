#!/usr/bin/env bash

# psp-devloop stage 02: run the in-game benchmark under PPSSPP, headless.

set -Eeuo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
game_dir="$project_dir/state/extremetuxracer/config/ppsspp/PSP/GAME/ExtremeTuxRacer"
benchmark_file="$game_dir/config/benchmark"
result_file="$game_dir/config/benchmark-result.json"
launcher_pid=

cleanup() {
  rm -f "$benchmark_file"
  if [[ -n "$launcher_pid" ]] && kill -0 "$launcher_pid" 2>/dev/null; then
    kill "$launcher_pid" 2>/dev/null || true
    wait "$launcher_pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

python3 "$project_dir/tools/stage-extremetuxracer.py"
mkdir -p "$game_dir/config"
rm -f "$result_file"
printf '600 frozen_river\n' >"$benchmark_file"

TUXRACER_BACKGROUND_AUDIO=0 timeout --signal=TERM 120 \
  "$project_dir/start-extremetuxracer-background.sh" &
launcher_pid=$!

for ((attempt = 0; attempt < 120; ++attempt)); do
  if [[ -s "$result_file" ]]; then
    python3 -m json.tool "$result_file" >/dev/null
    printf 'PASS: PPSSPP produced %s\n' "$result_file"
    exit 0
  fi

  if ! kill -0 "$launcher_pid" 2>/dev/null; then
    wait "$launcher_pid"
    printf 'FAIL: PPSSPP exited without a benchmark result\n' >&2
    exit 1
  fi

  sleep 1
done

printf 'FAIL: timed out waiting for PPSSPP benchmark result\n' >&2
exit 124
