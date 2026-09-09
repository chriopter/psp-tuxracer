# What is specific to this project. Sourced by both stage 02 runners after
# psp-devloop's lib/project.sh.
#
# The emulator is asked for a fixed benchmark run by dropping a request file
# next to the save data; the game writes benchmark-result.json when it reaches
# the end of the course.

ms0_dir="$project_dir/state/extremetuxracer/config/ppsspp/PSP"
game_dir="$ms0_dir/GAME/ExtremeTuxRacer"
built_eboot="$project_dir/ports/extremetuxracer/psp/EBOOT.PBP"
benchmark_file="$game_dir/config/benchmark"
result_file="$game_dir/config/benchmark-result.json"

# Restage whenever the build is newer than what was staged, so either runner
# works on its own without repeating the image conversion in the same loop.
stage() {
  if [[ ! -f "$game_dir/EBOOT.PBP" || "$built_eboot" -nt "$game_dir/EBOOT.PBP" ]]; then
    python3 "$project_dir/tools/stage-extremetuxracer.py"
  fi

  mkdir -p "$game_dir/config"
  printf '600 frozen_river\n' >"$benchmark_file"
}

check() {
  python3 -m json.tool "$result_file" >/dev/null
}

# Leaving the request file behind would make every later manual launch run the
# benchmark instead of the game.
cleanup() {
  rm -f "$benchmark_file"
}
