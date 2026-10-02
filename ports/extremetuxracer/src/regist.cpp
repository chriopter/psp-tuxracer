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

// PSP: a list of three rows. Up and down choose, left and right change the
// player or the character, Cross continues -- or, on the last row, opens
// the screen that registers a new player.
enum { ROW_PLAYER, ROW_CHARACTER, ROW_REGISTER, ROW_COUNT };
static int cursor_row = ROW_PLAYER;
static int sel_player = 0, sel_character = 0;
static bool confirmQuit = false;

static int wrap(int value, int count) {
	return count > 0 ? (value % count + count) % count : 0;
}

void QuitRegistration() {
	Players.ResetControls();
	Players.AllocControl(sel_player);
	g_game.player = Players.GetPlayer(sel_player);

	g_game.character = &Char.CharList[sel_character];
	Char.Ensure(*g_game.character);
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
	const int step = PspUI::ListKey(key, cursor_row, ROW_COUNT);
	if (step) {
		if (cursor_row == ROW_PLAYER) sel_player = wrap(sel_player + step, (int)Players.numPlayers());
		if (cursor_row == ROW_CHARACTER) sel_character = wrap(sel_character + step, (int)Char.CharList.size());
		return;
	}
	switch (key) {
		case sf::Keyboard::Escape:
			confirmQuit = true;
			break;
		case sf::Keyboard::Return:
			if (cursor_row == ROW_REGISTER) {
				g_game.player = Players.GetPlayer(sel_player);
				State::manager.RequestEnterState(NewPlayer);
			} else QuitRegistration();
			break;
		default:
			break;
	}
}

void CRegist::Mouse(int button, int state, int x, int y) {}
void CRegist::Motion(int x, int y) {}

void CRegist::Enter() {
	confirmQuit = false;
	Winsys.ShowCursor(false);
	Music.Play(param.menu_music, true);
	ResetGUI();
	cursor_row = ROW_PLAYER;
	sel_player = wrap((int)g_game.start_player, (int)Players.numPlayers());
	sel_character = wrap(sel_character, (int)Char.CharList.size());
}

void CRegist::Loop(float time_step) {
	ScopedRenderMode rm(GUI);
	Winsys.clear();

	if (param.ui_snow) {
		update_ui_snow(time_step);
		draw_ui_snow();
	}

	DrawGUIBackground(Winsys.scale);

	const TPlayer* tplayer = Players.GetPlayer(sel_player);
	std::vector<PspUI::Row> rows(ROW_COUNT);
	rows[ROW_PLAYER] = {Trans.Text(TXT_PLAYER), tplayer->name, Players.numPlayers() > 1};
	rows[ROW_CHARACTER] = {Trans.Text(TXT_CHARACTER), Char.CharList[sel_character].name, Char.CharList.size() > 1};
	rows[ROW_REGISTER] = {Trans.Text(61), "", false};
	PspUI::OptionList(40, 140, 440, rows, cursor_row);

	// Who is chosen, beside the list: the player's avatar and the character.
	const int size = 150;
	if (TTexture* picture = tplayer->avatar->Texture()) picture->DrawFrame(520, 142, size, size, 3, colWhite);
	if (TTexture* preview = Char.CharList[sel_character].Preview())
		preview->DrawFrame(520 + size + 20, 142, size, size, 3, colWhite);

	PspUI::Hint(44, 432, PspUI::Cross, cursor_row == ROW_REGISTER ? Trans.Text(TXT_NEW_PLAYER) : Trans.Text(TXT_CONTINUE));
	PspUI::Hint(320, 432, PspUI::Circle, Trans.Text(TXT_QUIT));
	PspUI::Hint(560, 432, PspUI::Dpad, Trans.Text(TXT_CHOOSE_CHANGE));
	if (confirmQuit) PspUI::Confirm(Trans.Text(TXT_QUIT_ASK), Trans.Text(TXT_QUIT_NOTE));

	Winsys.SwapBuffers();
}
