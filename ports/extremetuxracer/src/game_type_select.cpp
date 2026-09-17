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

#include "game_type_select.h"
#include "audio.h"
#include "ogl.h"
#include "textures.h"
#include "gui.h"
#include "particles.h"
#include "font.h"
#include "credits.h"
#include "translation.h"
#include "event_select.h"
#include "race_select.h"
#include "config_screen.h"
#include "help.h"
#include "score.h"
#include "winsys.h"
#include "savedata.hpp"
#include "psp_ui.h"
#include "regist.h"

CGameTypeSelect GameTypeSelect;

static TTextButton* textbuttons[8];
static sf::Sprite logo;
static bool confirmQuit = false;
static int selectedIndex = 0;

void EnterPractice() {
	g_game.game_type = PRACTICING;
	State::manager.RequestEnterState(RaceSelect);
}

void QuitGameType() {
	for (int i=0; i<8; ++i) if (textbuttons[i]->focussed()) selectedIndex = i;
	if (textbuttons[0]->focussed())
		State::manager.RequestEnterState(EventSelect);
	if (textbuttons[1]->focussed())
		EnterPractice();
	if (textbuttons[2]->focussed())
		State::manager.RequestEnterState(GameConfig);
	if (textbuttons[3]->focussed())
		State::manager.RequestEnterState(Score);
	if (textbuttons[4]->focussed())
		State::manager.RequestEnterState(Help);
	if (textbuttons[5]->focussed())
		State::manager.RequestEnterState(Credits);
	if (textbuttons[6]->focussed())
		PspSave::OpenMenu();
	if (textbuttons[7]->focussed())
		confirmQuit = true;
}

void CGameTypeSelect::Mouse(int button, int state, int x, int y) {
	if (state == 1) {
		if (confirmQuit) return;
		ClickGUI(x, y);
		QuitGameType();
	}
}

void CGameTypeSelect::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release) return;
	if (confirmQuit) {
		if (key == sf::Keyboard::Escape) confirmQuit = false;
		if (key == sf::Keyboard::Return) State::manager.RequestQuit();
		return;
	}

	switch (key) {
		case sf::Keyboard::U:
			param.ui_snow = !param.ui_snow;
			break;
		case sf::Keyboard::Escape:
			State::manager.RequestEnterState(Regist);
			break;
		case sf::Keyboard::Return:
			QuitGameType();
			break;
		case sf::Keyboard::W:
			Music.FreeMusics();
			break;
		default:
			KeyGUI(key, release);
			break;
	}
}

void CGameTypeSelect::Motion(int x, int y) {
	MouseMoveGUI(x, y);

	if (param.ui_snow) push_ui_snow(cursor_pos);
}

// ====================================================================

void CGameTypeSelect::Enter() {
	Winsys.ShowCursor(!param.ice_cursor);

	ResetGUI();
	confirmQuit = false;
	int top = 125;
	unsigned int siz = 27;
	int dist = 36;
	textbuttons[0] = AddTextButton(Trans.Text(1), 62, top, siz);
	textbuttons[1] = AddTextButton(Trans.Text(2), 62, top + dist, siz);
	textbuttons[2] = AddTextButton(Trans.Text(3), 62, top + dist * 2, siz);
	textbuttons[3] = AddTextButton(Trans.Text(62), 62, top + dist * 3, siz);
	textbuttons[4] = AddTextButton(Trans.Text(43), 62, top + dist * 4, siz);
	textbuttons[5] = AddTextButton(Trans.Text(4), 62, top + dist * 5, siz);
	textbuttons[6] = AddTextButton("Saved data", 62, top + dist * 6, siz);
	textbuttons[7] = AddTextButton(Trans.Text(5), 62, top + dist * 7, siz);
	SetFocus(textbuttons[selectedIndex]);
	logo.setTexture(Tex.GetSFTexture(T_TITLE));
	float logoScale = 170.0f / logo.getTextureRect().width;
	logo.setScale(logoScale, logoScale);
	logo.setPosition(650, 4);

	Music.Play(param.menu_music, true);
}

void CGameTypeSelect::Loop(float time_step) {
	ScopedRenderMode rm(GUI);
	Winsys.clear();

	PspUI::Background();
	Winsys.draw(logo);
	for (int i=0; i<8; ++i) {
		bool selected = textbuttons[i]->focussed();
		PspUI::Box(36,124+i*36,340,34,selected?sf::Color(30,72,98):sf::Color(18,36,53));
		if (selected) PspUI::Box(36,124+i*36,4,34,sf::Color(113,224,239));
	}
	PspUI::Controls(420,113);
	DrawGUI();
	PspUI::Hint(36,438,PspUI::Cross,"Select");
	PspUI::Hint(245,438,PspUI::Circle,"Player");
	PspUI::Hint(480,438,PspUI::Dpad,"Navigate");
	if (PspSave::NeedsAttention()) {
		PspUI::Text(38,405,"Autosave paused: check Saved data",18);
	}
	if (confirmQuit) PspUI::Confirm("QUIT GAME?", "Your saved progress is kept.");

	Winsys.SwapBuffers();
}
