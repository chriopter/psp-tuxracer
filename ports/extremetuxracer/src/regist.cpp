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

#include "regist.h"
#include "ogl.h"
#include "textures.h"
#include "audio.h"
#include "gui.h"
#include "particles.h"
#include "font.h"
#include "game_ctrl.h"
#include "translation.h"
#include "game_type_select.h"
#include "newplayer.h"
#include "winsys.h"
#include "savedata.hpp"
#include "psp_ui.h"
#include "controls_guide.h"

CRegist Regist;

static TWidget* textbuttons[2];
static TUpDown* player;
static TUpDown* character;
static bool confirmQuit = false;

void QuitRegistration() {
	Players.ResetControls();
	Players.AllocControl(player->GetValue());
	g_game.player = Players.GetPlayer(player->GetValue());

	g_game.character = &Char.CharList[character->GetValue()];
	PspSave::Save(false);
	Char.FreeCharacterPreviews(); // From here on, character previews are no longer required
	static bool guideShown = false;
	if (guideShown) State::manager.RequestEnterState(GameTypeSelect);
	else { guideShown = true; State::manager.RequestEnterState(ControlsGuide); }
}

void CRegist::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release) return;
	if (confirmQuit) {
		if (key == sf::Keyboard::Escape) confirmQuit = false;
		if (key == sf::Keyboard::Return) State::manager.RequestQuit();
		return;
	}
	TWidget* focussed = KeyGUI(key, release);
	switch (key) {
		case sf::Keyboard::Escape:
			confirmQuit = true;
			break;
		case sf::Keyboard::Return:
			if (focussed == textbuttons[1]) {
				g_game.player = Players.GetPlayer(player->GetValue());
				State::manager.RequestEnterState(NewPlayer);
			} else QuitRegistration();
			break;
		default:
			break;
	}
}

void CRegist::Mouse(int button, int state, int x, int y) {
	if (confirmQuit) return;
	if (state == 1) {
		TWidget* focussed = ClickGUI(x, y);
		if (focussed == textbuttons[0])
			QuitRegistration();
		else if (focussed == textbuttons[1]) {
			g_game.player = Players.GetPlayer(player->GetValue());
			State::manager.RequestEnterState(NewPlayer);
		}
	}
}

void CRegist::Motion(int x, int y) {
	MouseMoveGUI(x, y);

	if (param.ui_snow) push_ui_snow(cursor_pos);
}

static int framewidth, frameheight, arrowwidth;
static TArea area;
static float texsize;
static TLabel* sHelpPlayer;
static TLabel* sHelpCharacter;
static TFramedText* sPlayerFrame;
static TFramedText* sCharFrame;

void CRegist::Enter() {
	confirmQuit = false;
	Char.LoadCharacterPreviews();
	Winsys.ShowCursor(!param.ice_cursor);
	Music.Play(param.menu_music, true);

	framewidth = (int)(Winsys.scale * 280);
	frameheight = (int)(Winsys.scale * 50);
	arrowwidth = 70*Winsys.scale;
	int sumwidth = framewidth * 2 + arrowwidth * 2;
	area = AutoAreaN(30, 80, sumwidth);
	texsize = 128 * Winsys.scale;

	ResetGUI();
	player = AddUpDown(area.left + framewidth + 8, area.top, 0, (int)Players.numPlayers() - 1, (int)g_game.start_player);
	character = AddUpDown(area.left + framewidth * 2 + arrowwidth + 8, area.top, 0, (int)Char.CharList.size() - 1, 0);
	int siz = FT.AutoSizeN(5);
	textbuttons[0] = AddTextButton(Trans.Text(60), CENTER, AutoYPosN(62), siz);
	textbuttons[1] = AddTextButton(Trans.Text(61), CENTER, AutoYPosN(70), siz);

	FT.AutoSizeN(3);
	int top = AutoYPosN(24);
	sHelpPlayer = AddLabel(Trans.Text(58), area.left, top, colWhite);
	sHelpCharacter = AddLabel(Trans.Text(59), area.left + framewidth + arrowwidth, top, colWhite);

	FT.AutoSizeN(4);
	sPlayerFrame = AddFramedText(area.left, area.top, framewidth, frameheight, 3, colMBackgr, "", FT.GetSize());
	sCharFrame = AddFramedText(area.left + framewidth + arrowwidth, area.top, framewidth, frameheight, 3, colMBackgr, "", FT.GetSize());
}

void CRegist::Loop(float time_step) {
	ScopedRenderMode rm(GUI);
	Winsys.clear();

	if (param.ui_snow) {
		update_ui_snow(time_step);
		draw_ui_snow();
	}

	DrawGUIBackground(Winsys.scale);

	const TPlayer* tplayer = Players.GetPlayer(player->GetValue());
	sPlayerFrame->SetString(tplayer->name);
	sPlayerFrame->Focussed(player->focussed());
	tplayer->avatar->texture->DrawFrame(
	    area.left + 60, AutoYPosN(40), texsize, texsize, 3, colWhite);

	sCharFrame->SetString(Char.CharList[character->GetValue()].name);
	sCharFrame->Focussed(character->focussed());
	if (Char.CharList[character->GetValue()].preview != nullptr)
		Char.CharList[character->GetValue()].preview->DrawFrame(
		    area.right - texsize - 60 - arrowwidth,
		    AutoYPosN(40), texsize, texsize, 3, colWhite);

	DrawGUI();
	PspUI::Hint(44,432,PspUI::Cross,"Continue");
	PspUI::Hint(320,432,PspUI::Circle,"Quit");
	if (confirmQuit) PspUI::Confirm("QUIT GAME?", "Your saved progress is kept.");

	Winsys.SwapBuffers();
}
