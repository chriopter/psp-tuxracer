# Shared by the psp-devloop stage 02 runners. Sourced, not executed.
#
# The emulator is asked for a fixed benchmark run by dropping a request file
# next to the save data; the game writes benchmark-result.json when it reaches
# the end of the course. Waiting, timeout and process cleanup belong to
# psp-devloop's run-until-result.

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
devloop_root="${PSP_DEVLOOP_ROOT:-$project_dir/../psp-devloop}"
waiter="$devloop_root/lib/run-until-result"

ms0_dir="$project_dir/state/extremetuxracer/config/ppsspp/PSP"
game_dir="$ms0_dir/GAME/ExtremeTuxRacer"
built_eboot="$project_dir/ports/extremetuxracer/psp/EBOOT.PBP"
benchmark_file="$game_dir/config/benchmark"
result_file="$game_dir/config/benchmark-result.json"

if [[ ! -x "$waiter" ]]; then
  printf '[skip] psp-devloop run-until-result not found at %s\n' "$waiter"
  exit 77
fi

# Restage whenever the build is newer than what was staged, so either runner
# works on its own without repeating the image conversion in the same loop.
stage_if_stale() {
  if [[ ! -f "$game_dir/EBOOT.PBP" || "$built_eboot" -nt "$game_dir/EBOOT.PBP" ]]; then
    python3 "$project_dir/tools/stage-extremetuxracer.py"
  fi
}

# run_benchmark <emulator-name> <timeout-seconds> <command> [argument...]
run_benchmark() {
  local emulator="$1" timeout_seconds="$2"
  shift 2

  stage_if_stale
  mkdir -p "$game_dir/config"
  trap 'rm -f "$benchmark_file"' EXIT INT TERM
  printf '600 frozen_river\n' >"$benchmark_file"

  local status=0
  "$waiter" "$result_file" "$timeout_seconds" -- "$@" || status=$?

  case "$status" in
    0)
      python3 -m json.tool "$result_file" >/dev/null
      printf 'PASS: %s produced %s\n' "$emulator" "$result_file"
      ;;
    77) printf '[skip] %s is not available\n' "$emulator" ;;
    *)  printf 'FAIL: %s did not reach the benchmark checkpoint\n' "$emulator" >&2 ;;
  esac

  return "$status"
}
