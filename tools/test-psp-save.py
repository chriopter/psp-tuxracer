#!/usr/bin/env python3
"""Exercise the real PSP save codec, including damaged and oversized saves."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parent.parent
source = r'''
#include "save_format.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <sys/stat.h>
// INSERT_NATIVE_RESTORE
using namespace PspSaveFormat;
int main() {
  Files original{"[music_volume]25\n", "*[name]Racer[active]1\n", "*[pts]1234\n"};
  std::vector<uint8_t> packed;
  assert(encode(original, packed));
  Files restored;
  assert(decode(packed.data(), packed.size(), restored) && restored == original);
  // Every single-bit change in the header or payload must be detected.
  for (size_t i = 0; i < packed.size(); ++i) {
    for (int bit = 0; bit < 8; ++bit) {
      auto corrupt = packed; corrupt[i] ^= 1u << bit;
      Files destination = original;
      assert(!decode(corrupt.data(), corrupt.size(), destination));
      assert(destination == original);
    }
  }
  for (size_t size = 0; size < packed.size(); ++size)
    assert(!decode(packed.data(), size, restored));
  auto extra = packed; extra.push_back(0); assert(!decode(extra.data(), extra.size(), restored));
  Files huge{std::string(Capacity, 'x'), "", ""}; assert(!encode(huge, packed));
  Files exact{std::string(Capacity - HeaderSize, 'x'), "", ""};
  assert(encode(exact, packed) && decode(packed.data(), packed.size(), restored) && restored == exact);
  Files empty{}; assert(encode(empty, packed) && decode(packed.data(), packed.size(), restored) && restored == empty);
  Files nul{std::string("a\0b", 3), "", ""}; assert(!encode(nul, packed));
  assert(crc(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xcbf43926u);
  std::filesystem::create_directory("config");
  const Files initial{"original options", "original players", "original scores"};
  auto read = [](const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
  };
  auto unchanged = [&]() {
    for (int i = 0; i < 3; ++i)
      assert(read("config/" + std::string(names[i])) == initial[i]);
  };
  assert(writeFiles(original));
  assert(writeFiles(initial)); unchanged();
  // A failed temporary write must keep every original file.
  std::filesystem::create_directories("config/players.new/block");
  assert(!writeFiles(original)); unchanged();
  std::filesystem::remove_all("config/players.new");
  // Fail after two originals have moved to backups: both must roll back.
  std::filesystem::create_directories("config/highscore.bak/block");
  assert(!writeFiles(original)); unchanged();
  std::filesystem::remove_all("config/highscore.bak");
  assert(writeFiles(original));
  for (int i = 0; i < 3; ++i) {
    assert(read("config/" + std::string(names[i])) == original[i]);
    assert(!std::filesystem::exists("config/" + std::string(names[i]) + ".bak"));
    assert(!std::filesystem::exists("config/" + std::string(names[i]) + ".new"));
  }
  std::cout << "PASS: native local restore and rollback after write/rename failures\n";
  std::cout << "PASS: save round trip, full bit corruption, truncation, limits and unchanged output on failure\n";
}
'''
# Compile the actual local-restore functions, avoiding a separate test implementation.
native = (root/'ports/extremetuxracer/psp/savedata.cpp').read_text()
exists = native[native.index('static bool exists('):native.index('static bool releaseButtons(')]
restore = native[native.index('static bool writeFiles('):native.index('\nbool Save(')]
source = source.replace('// INSERT_NATIVE_RESTORE',
    'static const char *names[] = {"options.txt", "players", "highscore"};\n' + exists + restore)
with tempfile.TemporaryDirectory(prefix='etr-save-test-') as tmp:
    p = Path(tmp)
    (p/'test.cpp').write_text(source)
    subprocess.run(['g++', '-std=c++17', '-O2', '-fsanitize=undefined', '-I'+str(root/'ports/extremetuxracer/psp'), str(p/'test.cpp'), '-o', str(p/'test')], check=True)
    subprocess.run([str(p/'test')], cwd=p, check=True)
