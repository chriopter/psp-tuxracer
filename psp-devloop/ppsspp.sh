#!/usr/bin/env bash

# psp-devloop stage 02: run the in-game benchmark under PPSSPP, headless.

set -Eeuo pipefail
: "${PSP_DEVLOOP_ROOT:=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/psp-devloop}"
[[ -r "$PSP_DEVLOOP_ROOT/lib/project.sh" ]] ||
  { printf '[skip] psp-devloop not found at %s\n' "$PSP_DEVLOOP_ROOT"; exit 77; }
source "$PSP_DEVLOOP_ROOT/lib/project.sh"
source "$project_dir/psp-devloop/common.sh"

export TUXRACER_BACKGROUND_AUDIO=0

run_ppsspp 120 "$project_dir/start-extremetuxracer-background.sh"
