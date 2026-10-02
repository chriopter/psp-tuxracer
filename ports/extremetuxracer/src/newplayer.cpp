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

#include "newplayer.h"
#include "particles.h"
#include "audio.h"
#include "gui.h"
#include "ogl.h"
#include "textures.h"
#include "font.h"
#include "game_ctrl.h"
#include "translation.h"
#include "regist.h"
#include "winsys.h"
#include "spx.h"
#include "psp_ui.h"
#include "savedata.hpp"

CNewPlayer NewPlayer;

// PSP: three rows. Cross on the name opens the console's own keyboard,
// left and right on the avatar row turn through the pictures, Cross on the
// last row adds the player. Circle leaves without adding one.
enum { ROW_NAME, ROW_AVATAR, ROW_ADD, ROW_COUNT };
static int cursor_row = ROW_NAME;
static int sel_avatar = 0;
static std::string new_name;

static int wrap(int value, int count) {
	return count > 0 ? (value % count + count) % count : 0;
}

void QuitAndAddPlayer() {
	if (!new_name.empty())
		Players.AddPlayer(new_name, Players.GetDirectAvatarName(sel_avatar));
	State::manager.RequestEnterState(Regist);
}

void CNewPlayer::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release) return;
	const int step = PspUI::ListKey(key, cursor_row, ROW_COUNT);
	if (step) {
		if (cursor_row == ROW_AVATAR) sel_avatar = wrap(sel_avatar + step, (int)Players.numAvatars());
		return;
	}
	switch (key) {
		case sf::Keyboard::Escape:
			State::manager.RequestEnterState(Regist);
			break;
		case sf::Keyboard::Return:
			if (cursor_row == ROW_NAME) {
				std::string name = new_name;
				if (PspSave::EditPlayerName(name)) new_name = name;
			} else if (cursor_row == ROW_ADD) QuitAndAddPlayer();
			else cursor_row = ROW_ADD;
			break;
		default:
			break;
	}
}

void CNewPlayer::TextEntered(char text) {}
void CNewPlayer::Mouse(int button, int state, int x, int y) {}
void CNewPlayer::Motion(int x, int y) {}

void CNewPlayer::Enter() {
	Winsys.ShowCursor(false);
	Music.Play(param.menu_music, true);
	ResetGUI();
	cursor_row = ROW_NAME;
	sel_avatar = wrap(sel_avatar, (int)Players.numAvatars());
	new_name = "Player " + Int_StrN(Players.numPlayers()+1);
}

void CNewPlayer::Loop(float time_step) {
	ScopedRenderMode rm(GUI);
	Winsys.clear();

	if (param.ui_snow) {
		update_ui_snow(time_step);
		draw_ui_snow();
	}

	DrawGUIBackground(Winsys.scale);

	std::vector<PspUI::Row> rows(ROW_COUNT);
	rows[ROW_NAME] = {Trans.Text(TXT_NAME), new_name, false};
	rows[ROW_AVATAR] = {Trans.Text(TXT_AVATAR), Int_StrN(sel_avatar + 1) + " / " + Int_StrN((int)Players.numAvatars()), true};
	rows[ROW_ADD] = {Trans.Text(TXT_ADD_PLAYER), "", false};
	PspUI::OptionList(40, 140, 440, rows, cursor_row);

	if (TTexture* picture = Players.GetAvatarTexture(sel_avatar)) picture->DrawFrame(560, 142, 150, 150, 3,
	    cursor_row == ROW_AVATAR ? colDYell : colWhite);

	PspUI::Hint(44, 432, PspUI::Cross, cursor_row == ROW_NAME ? Trans.Text(TXT_EDIT_NAME) : cursor_row == ROW_ADD ? Trans.Text(TXT_ADD) : Trans.Text(TXT_NEXT));
	PspUI::Hint(320, 432, PspUI::Circle, Trans.Text(8));
	PspUI::Hint(560, 432, PspUI::Dpad, Trans.Text(TXT_CHOOSE_CHANGE));

	Winsys.SwapBuffers();
}
