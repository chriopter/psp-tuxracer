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
# PPSSPP's Linux shared-memory instance counter survives an abrupt stop.
# A stale counter makes the next launch mute itself as a secondary instance.
# Never reset it while another PPSSPP process is alive.
if [[ -d /dev/shm ]] && command -v pgrep >/dev/null; then
    if pgrep -ix 'ppsspp.*' >/dev/null; then
        printf '%s\n' 'Another PPSSPP instance is running; PPSSPP may mute this window. Close other instances for sound.' >&2
    elif [[ -f /dev/shm/PPSSPP_ID && -O /dev/shm/PPSSPP_ID && ! -L /dev/shm/PPSSPP_ID ]]; then
        if command -v fuser >/dev/null && fuser -s /dev/shm/PPSSPP_ID; then
            printf '%s\n' 'PPSSPP instance state is still in use; leaving it unchanged.' >&2
        else
            rm -f -- /dev/shm/PPSSPP_ID
        fi
    fi
fi
exec "$emulator_bin" --windowed --log="$project_dir/logs/etr-ppsspp.log" "$XDG_CONFIG_HOME/ppsspp/PSP/GAME/ExtremeTuxRacer/EBOOT.PBP"
