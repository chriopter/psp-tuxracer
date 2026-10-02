/* --------------------------------------------------------------------
EXTREME TUXRACER

Copyright (C) 2010 Extreme Tux Racer Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
---------------------------------------------------------------------*/
// PSP port modifications, 2026-09-07. See docs/porting.md in the port repository.


/*
If you want to add a new option, do this:
First add the option to the TParam struct (game_config.h).

Then edit the below functions:

- LoadConfigFile. Use
	SPIntN for integer and boolean values
	SPStrN for strings.
	The first value is always 'line', the second defines the tag within the
	brackets [ ], and the last value is the default.

- SetConfigDefaults. These values are used as long as no options file exists.
	It's a good idea to use the same values as the defaults in LoadConfigFile.

- SaveConfigFile. See the other entries; it should be self-explanatory.
	If an options file exists, you will have to change any value at runtime
	on the configuration screen to overwrite the file. Then you will see the
	new entry.
*/

#ifdef HAVE_CONFIG_H
#include <etr_config.h>
#endif

#include "config_screen.h"
#include "spx.h"
#include "translation.h"
#include "particles.h"
#include "audio.h"
#include "ogl.h"
#include "gui.h"
#include "font.h"
#include "winsys.h"
#include "psp_ui.h"
#include "savedata.hpp"

CGameConfig GameConfig;
static std::string res_names[NUM_RESOLUTIONS];

// PSP: the four settings that mean something on the console, one under the
// other. Up and down choose, left and right change, Cross saves, Circle
// leaves them as they were. Fullscreen and resolution are fixed on a PSP
// and no longer take two rows.
enum { ROW_MUSIC, ROW_SOUND, ROW_LANGUAGE, ROW_DETAIL, ROW_COUNT };
static int cursor_row = ROW_MUSIC;
static int sel_music = 0, sel_sound = 0, sel_language = 0, sel_detail = 1;

void SetConfig() {
	if (sel_music != param.music_volume || sel_sound != param.sound_volume ||
	        sel_language != (int)param.language || sel_detail != param.perf_level) {
		param.music_volume = sel_music;
		Music.SetVolume(param.music_volume);
		param.sound_volume = sel_sound;
		param.perf_level = sel_detail;
		FT.SetFontFromSettings();
		if ((int)param.language != sel_language) {
			param.language = sel_language;
			Trans.ChangeLanguage(param.language);
		}
		SaveConfigFile();
		PspSave::Save(false);
	}
	State::manager.RequestEnterState(*State::manager.PreviousState());
}

static int clampi(int value, int low, int high) {
	return value < low ? low : value > high ? high : value;
}

void CGameConfig::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release) return;
	const int step = PspUI::ListKey(key, cursor_row, ROW_COUNT);
	if (step) {
		const int languages = (int)Trans.languages.size();
		switch (cursor_row) {
			case ROW_MUSIC: sel_music = clampi(sel_music + 5 * step, 0, 100); break;
			case ROW_SOUND: sel_sound = clampi(sel_sound + 5 * step, 0, 100); break;
			case ROW_LANGUAGE: sel_language = languages > 0 ? ((sel_language + step) % languages + languages) % languages : 0; break;
			case ROW_DETAIL: sel_detail = clampi(sel_detail + step, 1, 4); break;
			default: break;
		}
		return;
	}
	if (key == sf::Keyboard::Escape)
		State::manager.RequestEnterState(*State::manager.PreviousState());
	else if (key == sf::Keyboard::Return)
		SetConfig();
}

void CGameConfig::Mouse(int button, int state, int x, int y) {}
void CGameConfig::Motion(int x, int y) {}

void CGameConfig::Enter() {
	Winsys.ShowCursor(false);
	ResetGUI();
	cursor_row = ROW_MUSIC;
	sel_music = param.music_volume;
	sel_sound = param.sound_volume;
	sel_language = (int)param.language;
	sel_detail = param.perf_level;
	Music.Play(param.config_music, true);
}

void CGameConfig::Loop(float time_step) {
	ScopedRenderMode rm(GUI);
	Winsys.clear();

	if (param.ui_snow) {
		update_ui_snow(time_step);
		draw_ui_snow();
	}

	DrawGUIBackground(Winsys.scale);

	std::vector<PspUI::Row> rows(ROW_COUNT);
	sf::String unused;
	PspUI::SplitLabel(Trans.Text(33), rows[ROW_MUSIC].label, unused);
	PspUI::SplitLabel(Trans.Text(34), rows[ROW_SOUND].label, unused);
	PspUI::SplitLabel(Trans.Text(35), rows[ROW_LANGUAGE].label, unused);
	PspUI::SplitLabel(Trans.Text(36), rows[ROW_DETAIL].label, unused);
	rows[ROW_MUSIC].value = Int_StrN(sel_music);
	rows[ROW_SOUND].value = Int_StrN(sel_sound);
	rows[ROW_LANGUAGE].value = Trans.languages[sel_language].language;
	rows[ROW_DETAIL].value = Int_StrN(sel_detail);
	for (auto& row : rows) row.adjustable = true;
	PspUI::OptionList(147, 140, 560, rows, cursor_row);

	PspUI::Hint(44, 432, PspUI::Cross, Trans.Text(TXT_SAVE));
	PspUI::Hint(320, 432, PspUI::Circle, Trans.Text(TXT_CANCEL));
	PspUI::Hint(560, 432, PspUI::Dpad, Trans.Text(TXT_CHOOSE_CHANGE));

	Winsys.SwapBuffers();
}
