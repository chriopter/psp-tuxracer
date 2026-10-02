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

#ifdef HAVE_CONFIG_H
#include <etr_config.h>
#endif

#include "course.h"
#include "event_select.h"
#include "gui.h"
#include "font.h"
#include "particles.h"
#include "audio.h"
#include "ogl.h"
#include "textures.h"
#include "game_ctrl.h"
#include "translation.h"
#include "event.h"
#include "game_type_select.h"
#include "opponents.h"
#include "psp_ui.h"
#include "winsys.h"

CEventSelect EventSelect;

// PSP: three rows, the event, the cup in it and the penguins to race. Up and down choose the row,
// left and right change it, Cross enters the cup if it is unlocked.
enum { ROW_EVENT, ROW_CUP, ROW_PENGUINS, ROW_COUNT };
static int cursor_row = ROW_EVENT;
static int sel_event = 0, sel_cup = 0;

static int wrap(int value, int count) {
	return count > 0 ? (value % count + count) % count : 0;
}

void EnterEvent() {
	g_game.game_type = CUPRACING;
	g_game.cup = Events.EventList[sel_event].cups[sel_cup];
	State::manager.RequestEnterState(Event);
}

void CEventSelect::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release) return;
	const int step = PspUI::ListKey(key, cursor_row, ROW_COUNT);
	if (step) {
		if (cursor_row == ROW_EVENT) {
			sel_event = wrap(sel_event + step, (int)Events.EventList.size());
			sel_cup = 0;
		} else if (cursor_row == ROW_CUP) sel_cup = wrap(sel_cup + step, (int)Events.EventList[sel_event].cups.size());
		else Opponents::Choose(wrap(Opponents::Chosen() + step, Opponents::MAX + 1));
		return;
	}
	if (key == sf::Keyboard::Escape)
		State::manager.RequestEnterState(GameTypeSelect);
	else if (key == sf::Keyboard::Return && Events.IsUnlocked(sel_event, sel_cup))
		EnterEvent();
}

void CEventSelect::Mouse(int button, int state, int x, int y) {}
void CEventSelect::Motion(int x, int y) {}

void CEventSelect::Enter() {
	Winsys.ShowCursor(false);

	/* FIXME: We should support events that use course group other than "default",
	 *        or what ever is set when we enter the event, but currently we won't.
	 *        Instead there's currently an assumption that the group is preset
	 *        correctly, so we set it here before entering event. Without this
	 *        it would crash if group is earlier changed to some non-default one. */
	Course.currentCourseList = &Course.CourseLists["default"];
	g_game.course = nullptr;

	ResetGUI();
	cursor_row = ROW_EVENT;
	sel_event = wrap(sel_event, (int)Events.EventList.size());
	sel_cup = wrap(sel_cup, (int)Events.EventList[sel_event].cups.size());

	Events.MakeUnlockList(g_game.player->funlocked);
	Music.Play(param.menu_music, true);
}

void CEventSelect::Loop(float time_step) {
	ScopedRenderMode rm(GUI);
	Winsys.clear();

	if (param.ui_snow) {
		update_ui_snow(time_step);
		draw_ui_snow();
	}

	DrawGUIBackground(Winsys.scale);

	const bool unlocked = Events.IsUnlocked(sel_event, sel_cup);
	std::vector<PspUI::Row> rows(ROW_COUNT);
	rows[ROW_EVENT] = {Trans.Text(TXT_EVENT), Events.EventList[sel_event].name, Events.EventList.size() > 1};
	rows[ROW_CUP] = {Trans.Text(TXT_CUP), Events.GetCupTrivialName(sel_event, sel_cup), Events.EventList[sel_event].cups.size() > 1};
	rows[ROW_PENGUINS] = {Trans.Text(TXT_VS_PENGUINS), Opponents::Chosen() ? sf::String(Int_StrN(Opponents::Chosen())) : Trans.Text(TXT_NONE), true};
	PspUI::OptionList(127, 130, 600, rows, cursor_row);

	if (!unlocked) {
		FT.SetSize(24);
		FT.SetColor(colLGrey);
		FT.DrawString(CENTER, 130 + ROW_COUNT * PspUI::RowHeight + 12, Trans.Text(10));
	}

	if (unlocked) PspUI::Hint(44, 432, PspUI::Cross, Trans.Text(TXT_CONTINUE));
	PspUI::Hint(320, 432, PspUI::Circle, Trans.Text(8));
	PspUI::Hint(560, 432, PspUI::Dpad, Trans.Text(TXT_CHOOSE_CHANGE));

	Winsys.SwapBuffers();
}
