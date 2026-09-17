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

CPaused Paused;

static bool sky = true;
static bool fog = true;
static bool terr = true;
static bool trees = true;

void CPaused::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release) return;
	switch (key) {
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
			g_game.raceaborted = true;
			g_game.race_result = -1;
			State::manager.RequestEnterState(GameOver);
			break;
		case sf::Keyboard::Return:
		case sf::Keyboard::P:
		case sf::Keyboard::Space:
			State::manager.RequestEnterState(Racing);
			break;
		default:
			break;
	}
}

void CPaused::Mouse(int button, int state, int x, int y) {
	State::manager.RequestEnterState(Racing);
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
		DrawFrameX((width - 570) / 2, 170, 570, 135, 2, colDBlue, colWhite, 0.85f);
		FT.SetColor(colWhite); FT.SetSize(34);
		FT.DrawString(CENTER, 188, "Paused");
		FT.SetSize(20);
		FT.DrawString(CENTER, 252, "Start: resume    Circle: end race");
	}
	Winsys.SwapBuffers();
}
