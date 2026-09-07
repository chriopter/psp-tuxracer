#!/usr/bin/env python3
"""Test the actual PSP music wrapper against a one-stream backend quota."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parent.parent
header = (root/'ports/extremetuxracer/psp/include/SFML/psp.hpp').read_text()
start = header.index('class Music {')
interface = header[start:header.index('\n};', start)+3]
platform = (root/'ports/extremetuxracer/psp/platform.cpp').read_text()
start = platform.index('struct Music::Data {')
implementation = platform[start:platform.index('\n} // namespace sf', start)]
source = r'''
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
struct Mix_Music { FILE *file; };
static int streams = 0, loads = 0;
static Mix_Music *playing = nullptr;
static bool failLoad = false, failPlay = false;
Mix_Music *Mix_LoadMUS(const char *path) {
  ++loads;
  if (failLoad || streams >= 1) return nullptr;
  FILE *f = fopen(path, "rb"); if (!f) return nullptr;
  ++streams; return new Mix_Music{f};
}
void Mix_FreeMusic(Mix_Music *m) {
  assert(m != playing); fclose(m->file); delete m; --streams;
}
void Mix_HaltMusic() { playing = nullptr; }
int Mix_PlayMusic(Mix_Music *m, int) {
  if (failPlay) return -1;
  playing = m; return 0;
}
void Mix_VolumeMusic(int) {}
const char *Mix_GetError() { return "simulated stream failure"; }
namespace sf {
INTERFACE
IMPLEMENTATION
}
int main() {
  {
    std::vector<sf::Music> tracks(10);
    for (int i = 0; i < 10; ++i) {
      std::string path = "track" + std::to_string(i);
      FILE *f = fopen(path.c_str(), "wb"); assert(f); fputs("test", f); fclose(f);
      assert(tracks[i].openFromFile(path));
    }
    assert(loads == 0 && streams == 0);
    tracks[0].play(); assert(streams == 1 && playing);
    auto active = playing;
    tracks[1].stop(); assert(streams == 1 && playing == active);
    for (int i = 1; i < 10001; ++i) {
      tracks[i % 10].play(); assert(streams == 1 && playing);
    }
    tracks[9].play(); assert(streams == 1 && playing);
    failLoad = true; tracks[0].play(); assert(streams == 0 && !playing);
    failLoad = false; tracks[0].play(); assert(streams == 1 && playing);
    failPlay = true; tracks[1].play(); assert(streams == 0 && !playing);
    failPlay = false; tracks[1].play(); assert(streams == 1 && playing);
    assert(!tracks[2].openFromFile("missing.wav"));
  }
  assert(streams == 0 && !playing);
  puts("PASS: ten tracks retain zero idle streams; 10,000 switches, failures and destruction respect a one-stream quota");
}
'''.replace('INTERFACE', interface).replace('IMPLEMENTATION', implementation)
with tempfile.TemporaryDirectory(prefix='etr-music-test-') as tmp:
    path = Path(tmp)
    (path/'test.cpp').write_text(source)
    subprocess.run(['g++', '-std=c++17', '-O2', '-fsanitize=undefined', str(path/'test.cpp'), '-o', str(path/'test')], check=True)
    subprocess.run([str(path/'test')], cwd=path, check=True)
