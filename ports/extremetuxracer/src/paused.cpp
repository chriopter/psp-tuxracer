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

#include "paused.h"
#include "audio.h"
#include "ogl.h"
#include "view.h"
#include "course_render.h"
#include "env.h"
#include "hud.h"
#include "track_marks.h"
#include "particles.h"
#include "textures.h"
#include "game_ctrl.h"
#include "tux.h"
#include "racing.h"
#include "winsys.h"
#include "physics.h"
#include "font.h"
#include "gui.h"
#include "game_over.h"
#include "psp_ui.h"

CPaused Paused;

static bool sky = true;
static bool fog = true;
static bool terr = true;
static bool trees = true;
static bool endSelected = false;
static bool confirmEnd = false;

void CPaused::Enter() { endSelected = false; confirmEnd = false; }

void CPaused::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release) return;
	if (confirmEnd) {
		if (key == sf::Keyboard::Escape || key == sf::Keyboard::P) confirmEnd = false;
		if (key == sf::Keyboard::Return) {
			g_game.raceaborted = true;
			g_game.race_result = -1;
			State::manager.RequestEnterState(GameOver);
		}
		return;
	}
	switch (key) {
		case sf::Keyboard::Up:
		case sf::Keyboard::Down:
			endSelected = !endSelected;
			break;
		case sf::Keyboard::C:
			Winsys.TakeScreenshot();
			break;
		case sf::Keyboard::F5:
			sky = !sky;
			break;
		case sf::Keyboard::F6:
			fog = !fog;
			break;
		case sf::Keyboard::F7:
			terr = !terr;
			break;
		case sf::Keyboard::F8:
			trees = !trees;
			break;
		case sf::Keyboard::Escape:
		case sf::Keyboard::P:
			State::manager.RequestEnterState(Racing);
			break;
		case sf::Keyboard::Return:
		case sf::Keyboard::Space:
			if (endSelected) confirmEnd = true;
			else State::manager.RequestEnterState(Racing);
			break;
		default:
			break;
	}
}

void CPaused::Mouse(int button, int state, int x, int y) {
	// PSP pause actions use explicit button presses, never incidental mouse input.
}

// ====================================================================

void CPaused::Loop(float time_step) {
	CControl *ctrl = g_game.player->ctrl;
	int width = Winsys.resolution.width;
	int height = Winsys.resolution.height;

	ClearRenderContext();
	Env.SetupFog();
	update_view(ctrl, 0);
	SetupViewFrustum(ctrl);

	if (sky) Env.DrawSkybox(ctrl->viewpos);
	if (fog) Env.DrawFog();
	Env.SetupLight();
	if (terr) RenderCourse();
	DrawTrackmarks();
	if (trees) DrawTrees();

	DrawSnow(ctrl);

	if (param.perf_level > 2) draw_particles(ctrl);
	g_game.character->shape->Draw();

	DrawHud(ctrl);
	Reshape(width, height);
	{
		ScopedRenderMode overlay(GUI);
		PspUI::Box(137,113,580,264,sf::Color(18,36,53));
		PspUI::Text(169,132,"PAUSED",34);
		PspUI::Box(163,endSelected?246:194,528,46,sf::Color(30,72,98));
		PspUI::Text(185,201,"Resume",28);
		PspUI::Text(185,253,"End race",28);
		PspUI::Hint(169,328,PspUI::Cross,"Select");
		PspUI::Hint(420,328,PspUI::Circle,"Resume");
		if (confirmEnd) PspUI::Confirm("END THIS RACE?", "This run will not count as a finish.");
	}
	Winsys.SwapBuffers();
}
