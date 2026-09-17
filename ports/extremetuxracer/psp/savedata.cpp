// SPDX-License-Identifier: GPL-2.0-or-later
// Native PSP savedata, 2026-09-07. One profile contains all players and scores.
#include "savedata.hpp"
#include "audio.h"
#include "font.h"
#include "game_config.h"
#include "game_ctrl.h"
#include "game_type_select.h"
#include "gui.h"
#include "ogl.h"
#include "regist.h"
#include "save_format.hpp"
#include "score.h"
#include "states.h"
#include "winsys.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <psputility.h>
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
    if (state == PSP_UTILITY_DIALOG_VISIBLE)
      sceUtilitySavedataUpdate(1);
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
  p.dataBuf = data.data();
  p.dataBufSize = p.dataSize = data.size();
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
  std::vector<uint8_t> data(PspSaveFormat::Capacity);
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
    if (state == PSP_UTILITY_DIALOG_VISIBLE)
      sceUtilityOskUpdate(1);
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
  TTextButton *save = nullptr, *load = nullptr, *back = nullptr;

public:
  void Enter() override {
    ResetGUI();
    save = AddTextButton("Save progress", CENTER, 205, 28);
    load = AddTextButton("Load progress", CENTER, 260, 28);
    back = AddTextButton("Back", CENTER, 315, 28);
  }
  void Keyb(sf::Keyboard::Key key, bool release, int, int) override {
    if (release)
      return;
    if (key == sf::Keyboard::Escape)
      State::manager.RequestEnterState(GameTypeSelect);
    else if (key == sf::Keyboard::Return) {
      if (save->focussed())
        Save(true);
      else if (load->focussed() && Load(true)) {
        Players.ResetControls();
        g_game.player = nullptr;
        Players.LoadPlayers();
        Score.LoadHighScore();
        InitConfig();
        Music.SetVolume(param.music_volume);
        State::manager.RequestEnterState(Regist);
      } else if (back->focussed())
        State::manager.RequestEnterState(GameTypeSelect);
    } else
      KeyGUI(key, release);
  }
  void Loop(float) override {
    ScopedRenderMode rm(GUI);
    Winsys.clear();
    DrawGUIFrame();
    FT.SetColor(colWhite);
    FT.SetSize(30);
    FT.DrawString(CENTER, 65, "Saved data");
    FT.SetSize(20);
    FT.DrawString(CENTER, 130, "Players, unlocked cups, scores and settings");
    FT.SetSize(18);
    FT.DrawString(CENTER, 390, status);
    FT.DrawString(CENTER, 435, "Cross: select    Circle: back");
    DrawGUI();
    Winsys.SwapBuffers();
  }
};
static SaveMenu menu;
void OpenMenu() { State::manager.RequestEnterState(menu); }
} // namespace PspSave
