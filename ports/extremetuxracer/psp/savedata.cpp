// SPDX-License-Identifier: GPL-2.0-or-later
// Native PSP savedata, 2026-09-07. One profile contains all players and scores.
// The savedata utility of current firmware takes the parameter block of
// firmware 2.00 and later, with the key that seals the save. The SDK gives
// the short 1.50 block unless told otherwise, and the console answers that
// one with 0x80110388, a parameter error, where the emulator let it pass.
#undef _PSP_FW_VERSION
#define _PSP_FW_VERSION 600
#include "savedata.hpp"
#include "audio.h"
#include "font.h"
#include "game_config.h"
#include "game_ctrl.h"
#include "game_type_select.h"
#include "gui.h"
#include "ogl.h"
#include "regist.h"
#include "psp_ui.h"
#include "translation.h"
#include "save_format.hpp"
#include "score.h"
#include "states.h"
#include "winsys.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <psputility.h>
#include <pspge.h>
#include <psputils.h>
#include <pspctrl.h>
#include <sys/stat.h>

namespace PspSave {
static bool benchmark = false, allowAuto = true;
static std::string status = "Progress is saved automatically between races.";
static const char *names[] = {"options.txt", "players", "highscore"};
static const char *nativePath =
    "ms0:/PSP/SAVEDATA/ETRX00001PROFILE/PROFILE.DAT";
void SetBenchmark(bool enabled) { benchmark = enabled; }
const std::string &Status() { return status; }
bool NeedsAttention() { return !allowAuto; }
static bool exists(const char *path) {
  struct stat s;
  return stat(path, &s) == 0;
}

// The firmware's dialogs draw with the GE as the game left it. After the
// clear that precedes a dialog's frame, PSPGL leaves the GE in clear mode
// (register 0xD3) until its next draw -- and in clear mode the GE writes
// plain vertex colour, no texture: on the console the save dialog came up
// as white boxes without a letter in them. The texture matrix, scale, blend
// and test settings PSPGL keeps are not what a dialog expects either. Around every update of a
// dialog the registers below are put to what a dialog expects and back to
// what PSPGL believes they hold.
static unsigned __attribute__((aligned(16))) geList[64];
// Register and the value a dialog is given: tests and effects off, texturing
// and plain alpha blending on, texture coordinates taken as they come, the
// colour table read whole, every colour bit written.
static const unsigned geWanted[][2] = {
    {0x17, 0}, {0x1D, 0}, {0x1E, 1}, {0x1F, 0}, {0x20, 0}, {0x21, 1}, {0x22, 0},
    {0x23, 0}, {0x24, 0}, {0x25, 0}, {0x26, 0}, {0x27, 0}, {0x28, 0},
    {0x48, 0x3F8000}, {0x49, 0x3F8000}, {0x4A, 0}, {0x4B, 0},   // scale 1.0, offset 0
    {0x50, 1}, {0xC0, 0}, {0xC1, 0}, {0xC2, 0},
    {0xC5, 0x00FF03}, {0xC6, 0x000101}, {0xC7, 0x000101}, {0xC8, 0},
    {0xC9, 0x000100},               // modulate, with the texture's alpha
    {0xCA, 0}, {0xD3, 0}, {0xD6, 0}, {0xD7, 0xFFFF},
    {0xDF, 0x000032},               // source alpha over one minus source alpha
    {0xE7, 0}, {0xE8, 0}, {0xE9, 0},
};
enum { geCount = sizeof(geWanted) / sizeof(geWanted[0]) };
static unsigned geSaved[geCount];

static void geRun(const unsigned *commands, int count) {
  for (int i = 0; i < count; ++i) geList[i] = commands[i];
  geList[count] = 0x0F000000u;      // FINISH
  geList[count + 1] = 0x0C000000u;  // END
  sceKernelDcacheWritebackRange(geList, sizeof(geList));
  const int id = sceGeListEnQueue(geList, geList + count + 2, -1, nullptr);
  if (id >= 0) sceGeListSync(id, 0);
}
static void dialogStateEnter() {
  unsigned wanted[geCount];
  for (int i = 0; i < geCount; ++i) {
    geSaved[i] = (geWanted[i][0] << 24) | (sceGeGetCmd(geWanted[i][0]) & 0x00FFFFFFu);
    // Clear mode is not put back: PSPGL has already asked for it to end.
    if (geWanted[i][0] == 0xD3) geSaved[i] = 0xD3000000u;
    wanted[i] = (geWanted[i][0] << 24) | geWanted[i][1];
  }
  geRun(wanted, geCount);
}
static void dialogStateLeave() { geRun(geSaved, geCount); }

static bool releaseButtons() {
  // The Cross press opening a utility must not type/confirm inside that utility.
  SceCtrlData pad{};
  while (PspIsRunning()) {
    sceCtrlPeekBufferPositive(&pad, 1);
    if (!pad.Buttons) return true;
    sceKernelDelayThread(10000);
  }
  return false;
}
// Sixteen bytes that seal this game's saves; any save written with them is
// read back only with them.
static const char saveKey[17] = "ETRX00001-PSP-01";

static int dialog(SceUtilitySavedataParam &p) {
  if (!releaseButtons()) return -1;
  p.base.size = sizeof(p);
  sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE,
                              &p.base.language);
  sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN,
                              &p.base.buttonSwap);
  p.base.graphicsThread = 0x11;
  p.base.accessThread = 0x13;
  p.base.fontThread = 0x12;
  p.base.soundThread = 0x10;
  std::strcpy(p.gameName, "ETRX00001");
  std::strcpy(p.saveName, "PROFILE");
  std::strcpy(p.fileName, "PROFILE.DAT");
  p.overwrite = 1;
  p.focus = PSP_UTILITY_SAVEDATA_FOCUS_LATEST;
  std::memcpy(p.key, saveKey, 16);
  p.sfoParam.parentalLevel = 1;
  std::strcpy(p.sfoParam.title, "Extreme Tux Racer PSP");
  std::strcpy(p.sfoParam.savedataTitle, "Players and progress");
  std::strcpy(p.sfoParam.detail,
              "Player profiles, unlocked cups, high scores and settings.");
  std::ifstream image("data/psp-icon.png", std::ios::binary);
  std::vector<char> icon((std::istreambuf_iterator<char>(image)), {});
  if (!icon.empty()) {
    p.icon0FileData.buf = icon.data();
    p.icon0FileData.bufSize = p.icon0FileData.size = icon.size();
  }
  int result = sceUtilitySavedataInitStart(&p);
  if (result < 0)
    return result;
  bool shuttingDown = false;
  for (;;) {
    ResetRenderMode();
    Winsys.clear();
    glFinish();
    int state = sceUtilitySavedataGetStatus();
    if (state == PSP_UTILITY_DIALOG_VISIBLE) {
      dialogStateEnter();
      sceUtilitySavedataUpdate(1);
      dialogStateLeave();
    }
    else if (state == PSP_UTILITY_DIALOG_QUIT && !shuttingDown) {
      sceUtilitySavedataShutdownStart();
      shuttingDown = true;
    } else if (state == PSP_UTILITY_DIALOG_NONE && shuttingDown)
      break;
    Winsys.SwapBuffers();
    sceKernelDelayThread(1000);
  }
  PspResetInput();
  return p.base.result;
}
static bool readFiles(PspSaveFormat::Files &files) {
  for (int i = 0; i < 3; ++i) {
    std::ifstream f("config/" + std::string(names[i]), std::ios::binary);
    if (!f)
      return false;
    files[i].assign(std::istreambuf_iterator<char>(f), {});
    if (f.bad())
      return false;
  }
  return true;
}
static bool writeFiles(const PspSaveFormat::Files &files) {
  std::array<std::string, 3> paths;
  std::array<bool, 3> backed{}, installed{};
  for (int i = 0; i < 3; ++i) paths[i] = "config/" + std::string(names[i]);
  auto rollback = [&]() {
    for (int i = 0; i < 3; ++i) {
      if (installed[i]) std::remove(paths[i].c_str());
      if (backed[i]) std::rename((paths[i] + ".bak").c_str(), paths[i].c_str());
      std::remove((paths[i] + ".new").c_str());
    }
    return false;
  };
  for (int i = 0; i < 3; ++i) {
    std::ofstream f(paths[i] + ".new", std::ios::binary);
    f.write(files[i].data(), files[i].size());
    f.close();
    if (!f) return rollback();
  }
  // Rename to absent destinations, also on PSP Memory Stick filesystems.
  // Retain all originals until the entire local restore has succeeded.
  for (int i = 0; i < 3; ++i) {
    if (!exists(paths[i].c_str())) continue;
    std::remove((paths[i] + ".bak").c_str());
    if (std::rename(paths[i].c_str(), (paths[i] + ".bak").c_str()) != 0)
      return rollback();
    backed[i] = true;
  }
  for (int i = 0; i < 3; ++i) {
    if (std::rename((paths[i] + ".new").c_str(), paths[i].c_str()) != 0)
      return rollback();
    installed[i] = true;
  }
  for (int i = 0; i < 3; ++i)
    if (backed[i]) std::remove((paths[i] + ".bak").c_str());
  return true;
}

bool Save(bool interactive) {
  if (benchmark || (!interactive && !allowAuto) || !g_game.player)
    return false;
  if (!SaveConfigFile() || !Players.SavePlayers() || !Score.SaveHighScore()) {
    status = "Could not prepare saved data. Check Memory Stick space.";
    return false;
  }
  PspSaveFormat::Files files;
  std::vector<uint8_t> data;
  if (!readFiles(files) || !PspSaveFormat::encode(files, data)) {
    status = "Could not prepare saved data. Your existing save was kept.";
    return false;
  }
  SceUtilitySavedataParam p{};
  p.mode =
      interactive ? PSP_UTILITY_SAVEDATA_SAVE : PSP_UTILITY_SAVEDATA_AUTOSAVE;
  // A sealed save is written in blocks of sixteen bytes: the buffer has
  // room for the last block filled up and for the seal after it.
  const std::size_t bytes = data.size();
  data.resize(((bytes + 15) & ~std::size_t(15)) + 16);
  p.dataBuf = data.data();
  p.dataBufSize = data.size();
  p.dataSize = bytes;
  int result = dialog(p);
  if (result == 0) {
    allowAuto = true;
    status = "Saved to the Memory Stick.";
    return true;
  }
  status = result == 1 ? "Save cancelled."
                       : "Save failed. Check Memory Stick space and try again.";
  std::fprintf(stderr, "PSP savedata save result: %08x\n", result);
  return false;
}
bool Load(bool interactive) {
  if (benchmark)
    return false;
  // Room for the padding and the seal a save of full size is written with.
  std::vector<uint8_t> data(PspSaveFormat::Capacity + 32);
  SceUtilitySavedataParam p{};
  p.mode =
      interactive ? PSP_UTILITY_SAVEDATA_LOAD : PSP_UTILITY_SAVEDATA_AUTOLOAD;
  p.dataBuf = data.data();
  p.dataBufSize = data.size();
  int result = dialog(p);
  if (result != 0) {
    status = result == 1
                 ? "Load cancelled."
                 : "Could not load saved data. Your current progress was kept.";
    std::fprintf(stderr, "PSP savedata load result: %08x\n", result);
    return false;
  }
  PspSaveFormat::Files files;
  if (!PspSaveFormat::decode(data.data(), p.dataSize, files)) {
    status = "Saved data is damaged or unsupported. It was not applied.";
    allowAuto = false;
    return false;
  }
  if (!writeFiles(files)) {
    status = "Could not restore local files. Native saved data was kept.";
    return false;
  }
  allowAuto = true;
  status = "Saved data loaded.";
  return true;
}
void LoadStartup() {
  if (benchmark)
    return;
  if (!exists(nativePath)) {
    allowAuto = true;
    return;
  }
  allowAuto = false;
  if (Load(false))
    InitConfig();
}

bool EditPlayerName(std::string &name) {
  if (!releaseButtons()) return false;
  unsigned short input[25]{}, output[25]{},
      description[] = {'P', 'l', 'a', 'y', 'e', 'r',
                       ' ', 'n', 'a', 'm', 'e', 0};
  for (size_t i = 0; i < std::min(size_t(24), name.size()); ++i)
    input[i] = (unsigned char)name[i];
  SceUtilityOskData field{};
  field.language = PSP_UTILITY_OSK_LANGUAGE_ENGLISH;
  field.inputtype = PSP_UTILITY_OSK_INPUTTYPE_LATIN_DIGIT |
                    PSP_UTILITY_OSK_INPUTTYPE_LATIN_LOWERCASE |
                    PSP_UTILITY_OSK_INPUTTYPE_LATIN_UPPERCASE;
  field.lines = 1;
  field.desc = description;
  field.intext = input;
  field.outtext = output;
  field.outtextlength = 25;
  field.outtextlimit = 24;
  SceUtilityOskParams p{};
  p.base.size = sizeof(p);
  sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE,
                              &p.base.language);
  sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN,
                              &p.base.buttonSwap);
  p.base.graphicsThread = 0x11;
  p.base.accessThread = 0x13;
  p.base.fontThread = 0x12;
  p.base.soundThread = 0x10;
  p.datacount = 1;
  p.data = &field;
  if (sceUtilityOskInitStart(&p) < 0)
    return false;
  bool shuttingDown = false;
  for (;;) {
    ResetRenderMode();
    Winsys.clear();
    glFinish();
    int state = sceUtilityOskGetStatus();
    if (state == PSP_UTILITY_DIALOG_VISIBLE) {
      dialogStateEnter();
      sceUtilityOskUpdate(1);
      dialogStateLeave();
    }
    else if (state == PSP_UTILITY_DIALOG_QUIT && !shuttingDown) {
      sceUtilityOskShutdownStart();
      shuttingDown = true;
    } else if (state == PSP_UTILITY_DIALOG_NONE && shuttingDown)
      break;
    Winsys.SwapBuffers();
    sceKernelDelayThread(1000);
  }
  PspResetInput();
  if (p.base.result != 0 || field.result == PSP_UTILITY_OSK_RESULT_CANCELLED)
    return false;
  std::string edited;
  for (int i = 0; i < 24 && output[i]; ++i) {
    unsigned c = output[i];
    // Keep profile delimiters and control characters out of the text format.
    if (c >= 32 && c <= 255 && c != '[' && c != ']' && c != '*')
      edited += char(c);
  }
  auto first = edited.find_first_not_of(' '),
       last = edited.find_last_not_of(' ');
  if (first == std::string::npos)
    return false;
  name = edited.substr(first, last - first + 1);
  return true;
}

class SaveMenu final : public State {
  // Two rows, as every other menu: up and down choose, Cross does it with
  // the console's own save or load dialog, Circle goes back.
  int cursor = 0;

public:
  void Enter() override {
    ResetGUI();
    cursor = 0;
  }
  void Keyb(sf::Keyboard::Key key, bool release, int, int) override {
    if (release)
      return;
    PspUI::ListKey(key, cursor, 2);
    if (key == sf::Keyboard::Escape)
      State::manager.RequestEnterState(GameTypeSelect);
    else if (key == sf::Keyboard::Return) {
      if (cursor == 0)
        Save(true);
      else if (Load(true)) {
        Players.ResetControls();
        g_game.player = nullptr;
        Players.LoadPlayers();
        Score.LoadHighScore();
        InitConfig();
        Music.SetVolume(param.music_volume);
        State::manager.RequestEnterState(Regist);
      }
    }
  }
  void Loop(float) override {
    ScopedRenderMode rm(GUI);
    Winsys.clear();
    DrawGUIBackground(Winsys.scale);
    FT.SetColor(colWhite);
    FT.SetSize(24);
    FT.DrawString(CENTER, 96, Trans.Text(TXT_SAVED_DATA));
    std::vector<PspUI::Row> rows(2);
    rows[0] = {Trans.Text(TXT_SAVE_PROGRESS), "", false};
    rows[1] = {Trans.Text(TXT_LOAD_PROGRESS), "", false};
    PspUI::OptionList(227, 150, 400, rows, cursor);
    FT.SetColor(colWhite);
    FT.SetSize(20);
    FT.DrawString(CENTER, 270, Trans.Text(TXT_SAVE_CONTENTS));
    FT.DrawString(CENTER, 310, status);
    PspUI::Hint(44, 432, PspUI::Cross, cursor == 0 ? Trans.Text(TXT_SAVE) : Trans.Text(TXT_LOAD));
    PspUI::Hint(320, 432, PspUI::Circle, Trans.Text(8));
    Winsys.SwapBuffers();
  }
};
static SaveMenu menu;
void OpenMenu() { State::manager.RequestEnterState(menu); }
} // namespace PspSave
