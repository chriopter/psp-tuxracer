/* --------------------------------------------------------------------
EXTREME TUXRACER

Copyright (C) 2010 Extreme Tux Racer Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
---------------------------------------------------------------------*/
// PSP port modifications, 2026-09-07. See docs/porting.md in the port repository.


#ifdef HAVE_CONFIG_H
#include <etr_config.h>
#endif

#include "credits.h"
#include "audio.h"
#include "ogl.h"
#include "particles.h"
#include "textures.h"
#include "font.h"
#include "gui.h"
#include "spx.h"
#include "winsys.h"

#define TOP_Y 165
#define BOTT_Y 64
#define FADE 50
#define OFFS_SCALE_FACTOR 1.2f

CCredits Credits;


static float y_offset = 0;
static bool moving = true;


void CCredits::LoadCreditList() {
	CSPList list;

	if (!list.Load(param.data_dir, "credits.lst")) {
		Message("could not load credits list");
		return;
	}

	std::forward_list<TCredits>::iterator last = CreditList.before_begin();
	for (CSPList::const_iterator line = list.cbegin(); line != list.cend(); ++line) {
		int old_offs = (last != CreditList.before_begin()) ? last->offs : 0;
		last = CreditList.emplace_after(last);
		TCredits& credit = *last;
		std::string temp = SPStrN(*line, "text");
		credit.text = sf::String::fromUtf8(temp.cbegin(), temp.cend());

		int offset = SPFloatN(*line, "offs", 0) * OFFS_SCALE_FACTOR * Winsys.scale;
		if (line != list.cbegin()) credit.offs = old_offs + offset;
		else credit.offs = offset;

		credit.col = SPIntN(*line, "col", 0);
		credit.size = SPFloatN(*line, "size", 1.f);
	}
}

void CCredits::DrawCreditsText(float time_step) {
	int h = Winsys.resolution.height;
	float offs = 0.f;
	if (moving) y_offset += time_step * 30;

	sf::Text text;
	text.setFont(FT.getCurrentFont());
	glEnable(GL_SCISSOR_TEST);
 glScissor(0, BOTT_Y * 272 / h, 480, (h-TOP_Y-BOTT_Y)*272/h);
	for (std::forward_list<TCredits>::const_iterator i = CreditList.begin(); i != CreditList.end(); ++i) {
		offs = h - TOP_Y - y_offset + i->offs;
		if (offs > h || offs < -100.f) // Draw only visible lines
			continue;

		if (i->col == 0) {
			text.setFillColor(colWhite);
			text.setOutlineColor(colWhite);
		} else {
			text.setFillColor(colDYell);
			text.setOutlineColor(colDYell);
		}
		text.setCharacterSize(FT.AutoSizeN(i->size)+1);
		text.setString(i->text);
		text.setPosition((Winsys.resolution.width - text.getLocalBounds().width) / 2, offs);
		Winsys.draw(text);
	}
	glDisable(GL_SCISSOR_TEST);

	if (offs < TOP_Y) y_offset = 0;
}

void CCredits::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release) return;
	switch (key) {
		case sf::Keyboard::M:
			moving = !moving;
			break;
		case sf::Keyboard::U:
			param.ui_snow = !param.ui_snow;
			break;
		default:
			State::manager.RequestEnterState(*State::manager.PreviousState());
	}
}

void CCredits::Mouse(int button, int state, int x, int y) {
	if (state == 1) State::manager.RequestEnterState(*State::manager.PreviousState());
}

void CCredits::Motion(int x, int y) {
	if (param.ui_snow) push_ui_snow(cursor_pos);
}

void CCredits::Enter() {
	LoadCreditList();

	Music.Play(param.credits_music, true);
	y_offset = 0;
	moving = true;

}

void CCredits::Exit() {

	CreditList.clear();
}

void CCredits::Loop(float time_step) {
	check_gl_error();
	ClearRenderContext();
	Winsys.clear();

	DrawCreditsText(time_step);
	if (param.ui_snow) {
		update_ui_snow(time_step);
		draw_ui_snow();
	}
	DrawGUIBackground(Winsys.scale);

	Winsys.SwapBuffers();
}
