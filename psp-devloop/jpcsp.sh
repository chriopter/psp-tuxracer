#!/usr/bin/env bash

# psp-devloop stage 02: the same benchmark under JPCSP.

set -Eeuo pipefail
: "${PSP_DEVLOOP_ROOT:=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/psp-devloop}"
[[ -r "$PSP_DEVLOOP_ROOT/lib/project.sh" ]] ||
  { printf '[skip] psp-devloop not found at %s\n' "$PSP_DEVLOOP_ROOT"; exit 77; }
source "$PSP_DEVLOOP_ROOT/lib/project.sh"
source "$project_dir/psp-devloop/common.sh"

# JPCSP needs roughly five times as long as PPSSPP to reach the checkpoint.
run_jpcsp 600 "$ms0_dir" "$game_dir/EBOOT.PBP"
