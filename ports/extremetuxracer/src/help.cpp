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
#include "psp_ui.h"

CHelp Help;

void CHelp::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release || (key != sf::Keyboard::Return && key != sf::Keyboard::Escape)) return;
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

}

void CHelp::Loop(float time_step) {
	ScopedRenderMode rm(GUI);
	Winsys.clear();

	PspUI::Background();
	PspUI::Text(36,113,"MENUS & RACING",24);
	PspUI::Hint(36,155,PspUI::Cross,"Confirm selection");
	PspUI::Hint(36,195,PspUI::Circle,"Back / cancel");
	PspUI::Text(36,245,"Left / right: select field",20);
	PspUI::Text(36,275,"Up / down: change value",20);
	PspUI::Text(36,325,"To leave a race:",22);
	PspUI::Text(36,355,"Pause, select End race,",20);
	PspUI::Text(36,382,"then confirm with Cross.",20);
	PspUI::Controls(420,113);
	PspUI::Hint(36,438,PspUI::Circle,"Back");
	PspUI::Hint(245,438,PspUI::Cross,"Back");
	Winsys.SwapBuffers();
}
