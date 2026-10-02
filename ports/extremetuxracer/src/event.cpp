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

#include "event.h"
#include "savedata.hpp"
#include "ogl.h"
#include "audio.h"
#include "particles.h"
#include "textures.h"
#include "gui.h"
#include "course.h"
#include "spx.h"
#include "font.h"
#include "game_ctrl.h"
#include "translation.h"
#include "event_select.h"
#include "game_over.h"
#include "game_config.h"
#include "loading.h"
#include "psp_ui.h"
#include "winsys.h"

CEvent Event;

// ready: 0 - racing  1 - ready with success  2 - ready with failure
static int ready = 0; // indicates if last race is done
static TCup *ecup = 0;
static std::size_t curr_race = 0;
static std::size_t curr_bonus = 0;
static sf::String info1, info2;

void StartRace() {
	if (ready > 0) {
		State::manager.RequestEnterState(EventSelect);
		return;
	}
	g_game.mirrorred = false;
	g_game.course = ecup->races[curr_race]->course;
	g_game.theme_id = ecup->races[curr_race]->music_theme;
	g_game.light_id = ecup->races[curr_race]->light;
	g_game.snow_id = ecup->races[curr_race]->snow;
	g_game.wind_id = ecup->races[curr_race]->wind;
	g_game.race = ecup->races[curr_race];
	g_game.game_type = CUPRACING;
	State::manager.RequestEnterState(Loading);
}

void CEvent::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release) return;
	switch (key) {
		case sf::Keyboard::Return:
			// Cross races the next course, or leaves a cup that is over.
			StartRace();
			break;
		case sf::Keyboard::Escape:
			State::manager.RequestEnterState(EventSelect);
			break;
		case sf::Keyboard::U:
			param.ui_snow = !param.ui_snow;
			break;
		default:
			break;
	}
}

void CEvent::Mouse(int button, int state, int x, int y) {}
void CEvent::Motion(int x, int y) {}

void InitCupRacing() {
	ecup = g_game.cup;
	curr_race = 0;
	curr_bonus = ecup->races.size();
	ready = 0;
}

void UpdateCupRacing() {
	std::size_t lastrace = ecup->races.size() - 1;
	curr_bonus += g_game.race_result;
	if (g_game.race_result >= 0) {
		if (curr_race < lastrace) curr_race++;
		else ready = 1;
	} else {
		if (curr_bonus == 0) ready = 2;
	}
	if (ready == 1) {
		Players.AddPassedCup(ecup->cup);
		Players.SavePlayers();
		PspSave::Save(false);
	}
}

// --------------------------------------------------------------------

// PSP layout, in the menus' 854x480: the cup's name, the row of bonus
// penguins, the races one under the other, and under them what the next
// race asks for. Cross and Circle are named at the foot.
enum { LIST_LEFT = 177, LIST_WIDTH = 500, LIST_TOP = 188, RACE_ROW = 40 };

void CEvent::Enter() {
	Winsys.ShowCursor(false);

	if (State::manager.PreviousState() == &GameOver) UpdateCupRacing();
	else InitCupRacing();

	ResetGUI();
	info1 = Trans.Text(11);
	info1 += "   " + Int_StrN(ecup->races[curr_race]->herrings.x);
	info1 += "   " + Int_StrN(ecup->races[curr_race]->herrings.y);
	info1 += "   " + Int_StrN(ecup->races[curr_race]->herrings.z);

	info2 = Trans.Text(12);
	info2 += "   " + Int_StrN((int)ecup->races[curr_race]->time.x);
	info2 += "   " + Int_StrN((int)ecup->races[curr_race]->time.y);
	info2 += "   " + Int_StrN((int)ecup->races[curr_race]->time.z);
	info2 += "  " + Trans.Text(14);

	Music.Play(param.menu_music, true);
}

int resultlevel(std::size_t num, std::size_t numraces) {
	if (num < 1) return 0;
	int q = (int)((num - 0.01) / numraces);
	return q + 1;
}

void CEvent::Loop(float time_step) {
	ScopedRenderMode rm(GUI);
	Winsys.clear();

	if (param.ui_snow) {
		update_ui_snow(time_step);
		draw_ui_snow();
	}
	DrawGUIBackground(Winsys.scale);

	const int bonustop = 136;
	if (ready == 0) {			// cup not finished
		FT.SetSize(30);
		FT.SetColor(colWhite);
		FT.DrawString(CENTER, 92, ecup->name);
		DrawBonusExt(bonustop, (int)ecup->races.size(), curr_bonus);

		const int races = (int)ecup->races.size();
		DrawFrameX(LIST_LEFT, LIST_TOP, LIST_WIDTH, races * RACE_ROW + 16, 3, colBackgr, colWhite, 1);

		TCheckbox checkbox(LIST_LEFT + LIST_WIDTH - 50, LIST_TOP, 32, "");
		FT.SetSize(26);
		for (std::size_t i=0; i<ecup->races.size(); i++) {
			int y = LIST_TOP + 8 + (int)i * RACE_ROW;
			FT.SetColor(i == curr_race ? colDYell : colWhite);
			FT.DrawString(LIST_LEFT + 24, y - 2, ecup->races[i]->course->name);
			checkbox.SetPosition(LIST_LEFT + LIST_WIDTH - 50, y + 2);
			checkbox.SetChecked(curr_race > i);
			checkbox.Draw();
		}
		// What the next race asks for, clear of the list and of the foot.
		const int below = LIST_TOP + races * RACE_ROW + 26;
		FT.SetSize(20);
		FT.SetColor(colWhite);
		FT.DrawString(CENTER, below, info1);
		FT.DrawString(CENTER, below + 26, info2);
	} else if (ready == 1) {		// cup successfully finished
		FT.SetSize(30);
		FT.SetColor(colWhite);
		FT.DrawString(CENTER, 230, Trans.Text(16));
		DrawBonusExt(bonustop, (int)ecup->races.size(), curr_bonus);
		int res = resultlevel(curr_bonus, ecup->races.size());
		FT.DrawString(CENTER, 280, Trans.Text(17) + " " + Int_StrN(res));
	} else if (ready == 2) {		// cup finished but failed
		FT.SetSize(30);
		FT.SetColor(colLRed);
		FT.DrawString(CENTER, 230, Trans.Text(18));
		DrawBonusExt(bonustop, ecup->races.size(), curr_bonus);
		FT.DrawString(CENTER, 280, Trans.Text(19));
	}

	if (ready < 1) {
		PspUI::Hint(44, 432, PspUI::Cross, Trans.Text(TXT_RACE));
		PspUI::Hint(320, 432, PspUI::Circle, Trans.Text(8));
	} else PspUI::Hint(44, 432, PspUI::Cross, Trans.Text(TXT_CONTINUE));

	Winsys.SwapBuffers();
}
