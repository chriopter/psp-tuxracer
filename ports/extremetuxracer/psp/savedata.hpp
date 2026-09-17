// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <string>
namespace PspSave {
void SetBenchmark(bool enabled);
void LoadStartup();
bool Save(bool interactive);
bool Load(bool interactive);
const std::string &Status();
bool NeedsAttention();
void OpenMenu();
bool EditPlayerName(std::string &name);
} // namespace PspSave
void PspResetInput();
bool PspIsRunning();
