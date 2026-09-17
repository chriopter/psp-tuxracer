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

#include "help.h"
#include "particles.h"
#include "audio.h"
#include "ogl.h"
#include "font.h"
#include "gui.h"
#include "translation.h"
#include "winsys.h"

CHelp Help;

#define TEXT_LINES 9
TLabel* headline;
TLabel* texts[TEXT_LINES];
TLabel* footnote;

void CHelp::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release) return;
	State::manager.RequestEnterState(*State::manager.PreviousState());
}

void CHelp::Mouse(int button, int state, int x, int y) {
	if (state == 1) State::manager.RequestEnterState(*State::manager.PreviousState());
}

void CHelp::Motion(int x, int y) {
	if (param.ui_snow) push_ui_snow(cursor_pos);
}

void CHelp::Enter() {
	ResetGUI();
	Winsys.ShowCursor(false);
	Music.Play(param.credits_music, true);

	int ytop = AutoYPosN(15);

	const int xleft1 = 40;

	FT.AutoSizeN(4);
	headline = AddLabel(Trans.Text(57), xleft1, AutoYPosN(5), colWhite);

	FT.AutoSizeN(3);
	int offs = FT.AutoDistanceN(2);
	const char* controls[]={"D-pad / analog stick: steer and navigate", "Up / R: paddle", "Down / L: brake", "Cross: jump / confirm", "Circle: back / end race", "Square + direction: trick", "Triangle: reset to course", "Start: pause / resume", "PPSSPP: remap host keys in Controls"};
 for (int i=0;i<TEXT_LINES;i++)texts[i]=AddLabel(controls[i],xleft1,ytop+offs*i,colWhite);

	footnote = AddLabel(Trans.Text(65), CENTER, AutoYPosN(90), colWhite);
}

void CHelp::Loop(float time_step) {
	ScopedRenderMode rm(GUI);
	Winsys.clear();

	if (param.ui_snow) {
		update_ui_snow(time_step);
		draw_ui_snow();
	}

	DrawGUI();
	Winsys.SwapBuffers();
}
