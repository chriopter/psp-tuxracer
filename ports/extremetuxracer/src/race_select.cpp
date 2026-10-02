/* --------------------------------------------------------------------
EXTREME TUXRACER

Copyright (C) 1999-2001 Jasmin F. Patry (Tuxracer)
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


#ifdef HAVE_CONFIG_H
#include <etr_config.h>
#endif

#include "race_select.h"
#include "ogl.h"
#include "textures.h"
#include "particles.h"
#include "audio.h"
#include "env.h"
#include "course.h"
#include "gui.h"
#include "font.h"
#include "translation.h"
#include "spx.h"
#include "game_type_select.h"
#include "loading.h"
#include "opponents.h"
#include "psp_ui.h"
#include "winsys.h"

CRaceSelect RaceSelect;

// PSP: one list, top to bottom. Up and down choose the row, left and right
// change it; Cross races, Circle goes back. The preview and the course's
// description stand beside the list.
enum { ROW_COURSE, ROW_GROUP, ROW_LIGHT, ROW_SNOW, ROW_WIND, ROW_MIRROR, ROW_VERSUS, ROW_PENGUINS, ROW_RANDOM, ROW_COUNT };
static int cursor_row = ROW_COURSE;
static int sel_group = 0, sel_course = 0;
static int sel_light = 0, sel_snow = 0, sel_wind = 0, sel_mirror = 0;

static int wrap(int value, int count) {
	return count > 0 ? (value % count + count) % count : 0;
}

void SetRaceConditions() {
	g_game.mirrorred = sel_mirror != 0;
	g_game.light_id = sel_light;
	g_game.snow_id = sel_snow;
	g_game.wind_id = sel_wind;

	g_game.course = &(*Course.currentCourseList)[sel_course];
	g_game.theme_id = (*Course.currentCourseList)[sel_course].music_theme;
	g_game.game_type = PRACTICING;
	State::manager.RequestEnterState(Loading);
}

void CRaceSelect::Motion(int x, int y) {}
void CRaceSelect::Mouse(int button, int state, int x, int y) {}

void CRaceSelect::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release) return;
	const int step = PspUI::ListKey(key, cursor_row, ROW_COUNT);
	if (step) {
		switch (cursor_row) {
			case ROW_COURSE:
				sel_course = wrap(sel_course + step, (int)Course.currentCourseList->size());
				break;
			case ROW_GROUP:
				sel_group = wrap(sel_group + step, (int)Course.CourseLists.size());
				Course.currentCourseList = Course.getGroup((std::size_t)sel_group);
				g_game.course = nullptr;
				sel_course = 0;
				break;
			case ROW_LIGHT: sel_light = wrap(sel_light + step, 4); break;
			case ROW_SNOW: sel_snow = wrap(sel_snow + step, 4); break;
			case ROW_WIND: sel_wind = wrap(sel_wind + step, 4); break;
			case ROW_MIRROR: sel_mirror = wrap(sel_mirror + step, 2); break;
			case ROW_VERSUS: Opponents::enabled = !Opponents::enabled; break;
			case ROW_PENGUINS:
				if (Opponents::enabled) Opponents::count = 1 + wrap(Opponents::count - 1 + step, Opponents::MAX);
				break;
			default: break;
		}
		return;
	}
	switch (key) {
		case sf::Keyboard::Escape:
			State::manager.RequestEnterState(GameTypeSelect);
			break;
		case sf::Keyboard::Return:
			if (cursor_row == ROW_VERSUS) {
				Opponents::enabled = !Opponents::enabled;
			} else if (cursor_row == ROW_RANDOM) {
				sel_mirror = IRandom(0, 1);
				sel_light = IRandom(0, 3);
				sel_snow = IRandom(0, 3);
				sel_wind = IRandom(0, 3);
			} else SetRaceConditions();
			break;
		default:
			break;
	}
}

void CRaceSelect::Enter() {
	Winsys.ShowCursor(false);
	Music.Play(param.menu_music, true);
	ResetGUI();
	cursor_row = ROW_COURSE;
	sel_light = wrap((int)g_game.light_id, 4);
	sel_snow = wrap((int)g_game.snow_id, 4);
	sel_wind = wrap((int)g_game.wind_id, 4);
	sel_mirror = g_game.mirrorred ? 1 : 0;
	// The set that is open stays open: find its place among the sets.
	for (int i = 0; i < (int)Course.CourseLists.size(); ++i)
		if (Course.getGroup((std::size_t)i) == Course.currentCourseList) sel_group = i;
	sel_course = g_game.course ? wrap((int)Course.GetCourseIdx(g_game.course), (int)Course.currentCourseList->size()) : 0;
}

void CRaceSelect::Loop(float time_step) {
	ScopedRenderMode rm(GUI);
	Winsys.clear();

	if (param.ui_snow) {
		update_ui_snow(time_step);
		draw_ui_snow();
	}

	DrawGUIBackground(Winsys.scale);

	TCourse& chosen = (*Course.currentCourseList)[sel_course];
	std::vector<PspUI::Row> rows(ROW_COUNT);
	rows[ROW_COURSE] = {Trans.Text(TXT_COURSE), chosen.name, true};
	rows[ROW_GROUP] = {Trans.Text(TXT_COURSE_SET), Course.currentCourseList->name, Course.CourseLists.size() > 1};
	PspUI::SplitLabel(Trans.Text(71 + sel_light), rows[ROW_LIGHT].label, rows[ROW_LIGHT].value);
	PspUI::SplitLabel(Trans.Text(75 + sel_snow), rows[ROW_SNOW].label, rows[ROW_SNOW].value);
	PspUI::SplitLabel(Trans.Text(79 + sel_wind), rows[ROW_WIND].label, rows[ROW_WIND].value);
	PspUI::SplitLabel(Trans.Text(69 + sel_mirror), rows[ROW_MIRROR].label, rows[ROW_MIRROR].value);
	for (int i = ROW_LIGHT; i <= ROW_MIRROR; ++i) rows[i].adjustable = true;
	rows[ROW_RANDOM] = {Trans.Text(83), "", false};
	// Racing against penguins: the tick, and how many while it is set.
	rows[ROW_VERSUS] = {Trans.Text(TXT_VS_SHORT), Trans.Text(Opponents::enabled ? TXT_ON : TXT_OFF), true};
	rows[ROW_PENGUINS] = {Trans.Text(TXT_PENGUINS), Opponents::enabled ? sf::String(Int_StrN(Opponents::count)) : sf::String("-"), Opponents::enabled};
	// The signs the game has always had for these, at the head of their rows.
	rows[ROW_LIGHT].icon = &Tex.GetSFTexture(LIGHT_BUTT); rows[ROW_LIGHT].icon_state = sel_light;
	rows[ROW_SNOW].icon = &Tex.GetSFTexture(SNOW_BUTT); rows[ROW_SNOW].icon_state = sel_snow;
	rows[ROW_WIND].icon = &Tex.GetSFTexture(WIND_BUTT); rows[ROW_WIND].icon_state = sel_wind;
	rows[ROW_MIRROR].icon = &Tex.GetSFTexture(MIRROR_BUTT); rows[ROW_MIRROR].icon_state = sel_mirror;
	rows[ROW_RANDOM].icon = &Tex.GetSFTexture(RANDOM_BUTT);
	// Seven rows fit above the key hints: the list moves under the cursor.
	enum { VISIBLE = 7 };
	static int first_row = 0;
	if (cursor_row < first_row) first_row = cursor_row;
	if (cursor_row >= first_row + VISIBLE) first_row = cursor_row - VISIBLE + 1;
	const std::vector<PspUI::Row> shown(rows.begin() + first_row, rows.begin() + first_row + VISIBLE);
	PspUI::OptionList(28, 100, 430, shown, cursor_row - first_row);

	// The course itself, beside the list: its picture, who made it, and
	// what it says about itself.
	const int px = 480, py = 104, pw = 256, ph = 192;
	if (TTexture* preview = Course.Preview(chosen)) preview->DrawFrame(px, py, pw, ph, 3, colWhite);
	FT.SetSize(18);
	FT.SetColor(colWhite);
	int line_y = py + ph + 12;
	if (cursor_row == ROW_VERSUS || cursor_row == ROW_PENGUINS) {
		// On the rows for the penguins, what they are instead.
		FT.DrawString(px, line_y, Trans.Text(TXT_VS_PENGUINS));
		FT.SetColor(colDYell);
		FT.DrawString(px, line_y + 24, Trans.Text(TXT_PSP_EXCLUSIVE));
	} else {
		sf::String author = Trans.Text(91);
		author.insert(author.getSize(), sf::String(": "));
		author.insert(author.getSize(), sf::String(chosen.author));
		FT.DrawString(px, line_y, author);
		for (std::size_t i = 0; i < chosen.num_lines && i < 3; i++)
			FT.DrawString(px, line_y + 24 + int(i) * 22, chosen.desc[i]);
	}

	PspUI::Hint(44, 432, PspUI::Cross, cursor_row == ROW_RANDOM ? Trans.Text(TXT_RANDOMIZE) : Trans.Text(TXT_RACE));
	PspUI::Hint(320, 432, PspUI::Circle, Trans.Text(8));
	PspUI::Hint(560, 432, PspUI::Dpad, Trans.Text(TXT_CHOOSE_CHANGE));

	Winsys.SwapBuffers();
}
