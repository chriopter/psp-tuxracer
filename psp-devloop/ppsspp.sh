#!/usr/bin/env bash

# psp-devloop stage 02: run the in-game benchmark under PPSSPP, headless.

set -Eeuo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/benchmark.sh"

export TUXRACER_BACKGROUND_AUDIO=0

run_benchmark PPSSPP 120 "$project_dir/start-extremetuxracer-background.sh"
