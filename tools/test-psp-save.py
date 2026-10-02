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
#include <cstring>
#include <algorithm>
#include <set>
// ---- host stand-ins for the PSP SDK and the game around psp/savedata.cpp ----
struct SceCtrlData { unsigned Buttons = 0; };
static bool PspIsRunning() { return true; }
static void sceCtrlPeekBufferPositive(SceCtrlData *pad, int) { pad->Buttons = 0; }
static void sceKernelDelayThread(unsigned) {}
static void PspResetInput() {}
static void ResetRenderMode() {}
static void glFinish() {}
static struct { void clear() {} void SwapBuffers() {} } Winsys;
static int playerObject;
static struct { int *player = &playerObject; } g_game;
static bool prepareOk = true;
static int initConfigCalls = 0;
static bool SaveConfigFile() { return prepareOk; }
static void InitConfig() { ++initConfigCalls; }
static struct { bool SavePlayers() { return prepareOk; } } Players;
static struct { bool SaveHighScore() { return prepareOk; } } Score;
enum { PSP_SYSTEMPARAM_ID_INT_LANGUAGE, PSP_SYSTEMPARAM_ID_INT_UNKNOWN,
       PSP_UTILITY_SAVEDATA_AUTOLOAD = 0, PSP_UTILITY_SAVEDATA_AUTOSAVE,
       PSP_UTILITY_SAVEDATA_LOAD, PSP_UTILITY_SAVEDATA_SAVE,
       PSP_UTILITY_SAVEDATA_FOCUS_LATEST = 4,
       PSP_UTILITY_DIALOG_NONE = 0, PSP_UTILITY_DIALOG_INIT, PSP_UTILITY_DIALOG_VISIBLE,
       PSP_UTILITY_DIALOG_QUIT, PSP_UTILITY_DIALOG_FINISHED };
static int sceUtilityGetSystemParamInt(int, int *value) { *value = 1; return 0; }
struct SceUtilitySavedataParam {
  struct { unsigned size; int language, buttonSwap, graphicsThread, accessThread,
           fontThread, soundThread, result; } base;
  int mode, overwrite, focus;
  char gameName[13], saveName[20], fileName[13];
  void *dataBuf; std::size_t dataBufSize, dataSize;
  struct { char title[0x80], savedataTitle[0x80], detail[0x400]; unsigned char parentalLevel; } sfoParam;
  struct { void *buf; std::size_t bufSize, size; } icon0FileData;
  char key[16];
};

// The GE: one 24-bit value per register, changed only by an enqueued list.
static unsigned geRegister[256];
static const void *flushedList; static std::size_t flushedBytes;
static int geLists = 0, geSyncs = 0;
static void sceKernelDcacheWritebackRange(const void *p, unsigned n) { flushedList = p; flushedBytes = n; }
static unsigned sceGeGetCmd(int cmd) { return (unsigned(cmd) << 24) | geRegister[cmd]; }
static int sceGeListEnQueue(const void *list, void *stall, int, void *) {
  const unsigned *w = static_cast<const unsigned *>(list);
  // The GE reads memory: the list must have left the CPU cache, whole.
  assert(reinterpret_cast<std::uintptr_t>(list) % 16 == 0);
  assert(flushedList == list);
  int n = 0;
  for (; (w[n] >> 24) != 0x0C; ++n) {
    assert(std::size_t(n + 1) * sizeof(unsigned) < flushedBytes);
    if ((w[n] >> 24) == 0x0F) { assert((w[n + 1] >> 24) == 0x0C); continue; }
    geRegister[w[n] >> 24] = w[n] & 0x00FFFFFFu;
  }
  assert(n >= 1 && (w[n - 1] >> 24) == 0x0F);     // FINISH, then END
  assert(stall == w + n + 1);
  flushedList = nullptr;
  return 100 + geLists++;
}
static int sceGeListSync(int id, int mode) { assert(id == 100 + geLists - 1 && mode == 0); ++geSyncs; return 0; }

// A Memory Stick and the savedata utility in front of it.
struct UtilityCall {
  int mode; std::size_t bufSize, dataSize, structSize; bool keyed, keyEmpty;
  std::vector<uint8_t> buffer; std::string game, save, file;
};
static std::vector<UtilityCall> calls;
static std::vector<uint8_t> stick; static bool stickPresent = false, stickKeyed = false;
static int forcedResult = 0;          // what the user or the firmware answers
static bool forceEveryCall = false;
static SceUtilitySavedataParam *active; static int utilityState = PSP_UTILITY_DIALOG_NONE;
static void (*duringDialog)() = nullptr; static int dialogUpdates = 0;
static int sceUtilitySavedataInitStart(SceUtilitySavedataParam *p) {
  const auto *bytes = static_cast<const uint8_t *>(p->dataBuf);
  UtilityCall c{p->mode, p->dataBufSize, p->dataSize, p->base.size,
                !std::memcmp(p->key, "ETRX00001-PSP-01", 16),
                std::all_of(p->key, p->key + 16, [](char k) { return !k; }),
                {bytes, bytes + p->dataBufSize}, p->gameName, p->saveName, p->fileName};
  calls.push_back(c);
  assert(c.keyed || c.keyEmpty);
  int result = forcedResult;
  if (!forceEveryCall) forcedResult = 0;
  const bool saving = p->mode == PSP_UTILITY_SAVEDATA_SAVE || p->mode == PSP_UTILITY_SAVEDATA_AUTOSAVE;
  if (!result && saving) {
    // A sealed save needs whole 16-byte blocks and room for the seal.
    if (c.keyed && (c.bufSize % 16 || c.bufSize < ((c.dataSize + 15) / 16) * 16 + 16)) result = 0x80110388;
    else { stick.assign(bytes, bytes + c.dataSize); stickPresent = true; stickKeyed = c.keyed; }
  } else if (!result) {
    if (!stickPresent) result = 0x80110307;
    else if (stickKeyed != c.keyed) result = 0x80110306;  // sealed otherwise: reads as damaged
    else if (stick.size() > c.bufSize) result = 0x80110388;
    else { std::memcpy(p->dataBuf, stick.data(), stick.size()); p->dataSize = stick.size(); }
  }
  p->base.result = result;
  active = p; utilityState = PSP_UTILITY_DIALOG_VISIBLE;
  return 0;
}
static int sceUtilitySavedataGetStatus() { return utilityState; }
static void sceUtilitySavedataUpdate(int) {
  ++dialogUpdates;
  if (duringDialog) duringDialog();
  utilityState = PSP_UTILITY_DIALOG_QUIT;
}
static void sceUtilitySavedataShutdownStart() { utilityState = PSP_UTILITY_DIALOG_NONE; }
// INSERT_NATIVE
using namespace PspSaveFormat;

static unsigned geBefore[256];
static bool listed(int cmd) {
  for (int i = 0; i < geCount; ++i) if (int(geWanted[i][0]) == cmd) return true;
  return false;
}
static void expectDialogState() {
  for (int i = 0; i < geCount; ++i) assert(geRegister[geWanted[i][0]] == geWanted[i][1]);
  assert(geRegister[0xD3] == 0);                 // clear mode off: dialogs are textured
  for (int cmd = 0; cmd < 256; ++cmd) if (!listed(cmd)) assert(geRegister[cmd] == geBefore[cmd]);
}
static void expectGameState() {
  for (int cmd = 0; cmd < 256; ++cmd)
    assert(geRegister[cmd] == (cmd == 0xD3 ? 0u : geBefore[cmd]));
}
static void scrambleGe(unsigned seed) {
  for (int cmd = 0; cmd < 256; ++cmd) {
    seed = seed * 1664525u + 1013904223u;
    geBefore[cmd] = geRegister[cmd] = (seed >> 8) & 0x00FFFFFFu;
  }
  // The case from the console: PSPGL left the GE in clear mode.
  geBefore[0xD3] = geRegister[0xD3] = 0x000701;
}
static std::string slurp(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f), {});
}
static void place(const Files &files) {
  for (int i = 0; i < 3; ++i) {
    std::ofstream f("config/" + std::string(names[i]), std::ios::binary);
    f.write(files[i].data(), files[i].size());
  }
}
static void native_test() {
  // --- GE registers around a dialog ---
  assert(geCount + 2 <= int(sizeof(geList) / sizeof(geList[0])));
  std::set<unsigned> distinct;
  for (int i = 0; i < geCount; ++i) {
    assert(geWanted[i][0] < 256 && geWanted[i][1] <= 0x00FFFFFFu);
    assert(distinct.insert(geWanted[i][0]).second);
  }
  assert(listed(0xD3));
  for (unsigned seed = 1; seed <= 20; ++seed) {
    scrambleGe(seed);
    const int lists = geLists, syncs = geSyncs;
    dialogStateEnter(); expectDialogState();
    dialogStateLeave(); expectGameState();
    assert(geLists == lists + 2 && geSyncs == syncs + 2);
    // Twice in a row, as in every frame of a dialog: still the game's values.
    for (int cmd = 0; cmd < 256; ++cmd) geBefore[cmd] = geRegister[cmd];
    dialogStateEnter(); expectDialogState();
    dialogStateLeave(); expectGameState();
  }

  // --- the buffer handed to the utility when saving ---
  std::filesystem::create_directory("config");
  std::set<std::size_t> remainders;
  for (int extra = 0; extra < 33; ++extra) {
    const Files files{std::string(extra, 'o'), "*[name]Racer\n", "*[pts]1\n"};
    place(files);
    std::vector<uint8_t> expected; assert(encode(files, expected));
    remainders.insert(expected.size() % 16);
    for (bool interactive : {false, true}) {
      calls.clear(); scrambleGe(extra + 50); dialogUpdates = 0; duringDialog = expectDialogState;
      assert(PspSave_Save(interactive));
      duringDialog = nullptr;
      assert(dialogUpdates == 1); expectGameState();
      assert(calls.size() == 1);
      const auto &c = calls[0];
      assert(c.mode == (interactive ? PSP_UTILITY_SAVEDATA_SAVE : PSP_UTILITY_SAVEDATA_AUTOSAVE));
      assert(c.structSize == sizeof(SceUtilitySavedataParam));
      assert(c.dataSize == expected.size());                 // the true size, not the padded one
      assert(c.bufSize % 16 == 0);
      assert(c.bufSize == (expected.size() + 15) / 16 * 16 + 16);
      assert(c.bufSize >= c.dataSize + 16 && c.bufSize < c.dataSize + 32);
      assert(std::equal(expected.begin(), expected.end(), c.buffer.begin()));
      for (std::size_t i = c.dataSize; i < c.bufSize; ++i) assert(c.buffer[i] == 0);
      assert(c.keyed && c.game == "ETRX00001" && c.save == "PROFILE" && c.file == "PROFILE.DAT");
      assert(stick == expected && stickKeyed && Status() == "Saved to the Memory Stick.");
    }
  }
  assert(remainders.size() == 16);                           // every remainder, 0 included

  // --- loading: sealed save, save from before the key, failures ---
  const Files saved{"[music_volume]25\n", "*[name]Racer[active]1\n", "*[pts]1234\n"};
  const Files local{"local options", "local players", "local scores"};
  auto localIs = [](const Files &f) {
    for (int i = 0; i < 3; ++i) if (slurp("config/" + std::string(names[i])) != f[i]) return false;
    return true;
  };
  place(saved); calls.clear(); assert(PspSave_Save(false));
  for (bool interactive : {false, true}) {
    place(local); calls.clear();
    assert(PspSave_Load(interactive) && localIs(saved));
    assert(calls.size() == 1 && calls[0].keyed);
    assert(calls[0].mode == (interactive ? PSP_UTILITY_SAVEDATA_LOAD : PSP_UTILITY_SAVEDATA_AUTOLOAD));
    assert(calls[0].bufSize == Capacity + 32);
  }
  // A save not sealed with this game's key is not read: one attempt, with
  // the key, and nothing applied. There is no second, unsealed attempt.
  for (bool interactive : {false, true}) {
    stickKeyed = false; place(local); calls.clear();
    assert(!PspSave_Load(interactive) && calls.size() == 1 && calls[0].keyed && localIs(local));
  }
  stickKeyed = true; calls.clear(); forcedResult = 1;
  assert(!PspSave_Load(false) && calls.size() == 1 && Status() == "Load cancelled." && localIs(local));
  // A failing utility: one attempt, nothing applied, and the next load works.
  calls.clear(); forcedResult = 0x80110306; forceEveryCall = true;
  assert(!PspSave_Load(false) && calls.size() == 1 && calls[0].keyed && localIs(local));
  forcedResult = 0; forceEveryCall = false;
  calls.clear(); assert(PspSave_Load(false) && calls.size() == 1 && calls[0].keyed && localIs(saved));
  // No save at all: one attempt.
  stickPresent = false; place(local); calls.clear();
  assert(!PspSave_Load(false) && calls.size() == 1 && localIs(local));
  // A damaged save is never applied and stops the autosave from replacing it.
  stickPresent = true; stickKeyed = true; stick[30] ^= 1; calls.clear();
  assert(!PspSave_Load(false) && calls.size() == 1 && localIs(local) && NeedsAttention());
  calls.clear(); assert(!PspSave_Save(false) && calls.empty());
  assert(PspSave_Save(true) && calls.size() == 1 && calls[0].keyed && !NeedsAttention());
  // Nothing is handed over when the local files could not be prepared, or cancelled.
  prepareOk = false; calls.clear(); assert(!PspSave_Save(true) && calls.empty()); prepareOk = true;
  const auto kept = stick; forcedResult = 1;
  assert(!PspSave_Save(true) && Status() == "Save cancelled." && stick == kept);
  g_game.player = nullptr; calls.clear(); assert(!PspSave_Save(true) && calls.empty());
  g_game.player = &playerObject;
  std::filesystem::remove_all("config");
  std::cout << "PASS: save buffer in 16-byte blocks plus seal, key on every call, no unsealed retry, GE registers restored around dialogs\n";
}
int main() {
  native_test();
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
# Compile the actual save, load, dialog and local-restore functions, avoiding
# a separate test implementation. Everything between the file-scope state and
# the on-screen keyboard is taken as it is.
native = (root/'ports/extremetuxracer/psp/savedata.cpp').read_text()
# The savedata block with the key exists only from firmware 2.00 on, and the
# SDK headers decide that when they are first included.
assert native.index('#define _PSP_FW_VERSION 600') < native.index('#include'), 'firmware version must precede every include'
body = native[native.index('static bool benchmark'):native.index('\nbool EditPlayerName(')]
assert 'dialogStateEnter();\n      sceUtilitySavedataUpdate(1);\n      dialogStateLeave();' in body
for name in ('Save', 'Load'):
    assert f'\nbool {name}(bool interactive) {{' in body
    body = body.replace(f'\nbool {name}(bool interactive) {{', f'\nbool PspSave_{name}(bool interactive) {{')
body = body.replace('if (Load(false))', 'if (PspSave_Load(false))')
source = source.replace('// INSERT_NATIVE', body)
with tempfile.TemporaryDirectory(prefix='etr-save-test-') as tmp:
    p = Path(tmp)
    (p/'test.cpp').write_text(source)
    subprocess.run(['g++', '-std=c++17', '-O2', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-I'+str(root/'ports/extremetuxracer/psp'), str(p/'test.cpp'), '-o', str(p/'test')], check=True)
    subprocess.run([str(p/'test')], cwd=p, check=True)
