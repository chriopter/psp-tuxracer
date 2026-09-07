#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export XDG_CONFIG_HOME="$project_dir/state/extremetuxracer/config"
export XDG_DATA_HOME="$project_dir/state/extremetuxracer/data"
export XDG_CACHE_HOME="$project_dir/state/extremetuxracer/cache"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$project_dir/logs"
cd "$project_dir"
# Use the desktop audio server explicitly; the background launcher can select dummy.
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-pulseaudio}"
export SDL_AUDIO_DRIVER="$SDL_AUDIODRIVER"
export LD_LIBRARY_PATH="$project_dir/emulator/host-libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if [[ -n "${PPSSPP_BIN:-}" ]]; then
    emulator_bin="$PPSSPP_BIN"
elif [[ -x "$project_dir/emulator/AppDir/shared/bin/PPSSPPSDL" ]]; then
    emulator_bin="$project_dir/emulator/AppDir/shared/bin/PPSSPPSDL"
elif command -v PPSSPPSDL >/dev/null; then
    emulator_bin="$(command -v PPSSPPSDL)"
else
    echo 'Install PPSSPPSDL or set PPSSPP_BIN to its executable.' >&2
    exit 1
fi
exec "$emulator_bin" --windowed --log="$project_dir/logs/etr-ppsspp.log" "$XDG_CONFIG_HOME/ppsspp/PSP/GAME/ExtremeTuxRacer/EBOOT.PBP"
