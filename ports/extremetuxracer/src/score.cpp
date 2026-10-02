/* --------------------------------------------------------------------
EXTREME TUXRACER

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

#include "score.h"
#include "ogl.h"
#include "audio.h"
#include "gui.h"
#include "particles.h"
#include "font.h"
#include "game_ctrl.h"
#include "translation.h"
#include "course.h"
#include "spx.h"
#include "psp_ui.h"
#include "winsys.h"

CScore Score;

int CScore::AddScore(const std::string& group, const std::string& course, TScore&& score) {
	if (score.points < 1) return 999;

	TScoreList *list = &Scorelist[group][course];
	int num = list->numScores;
	int pos = 0;
	int lastpos = num-1;
	int val = score.points;

	if (num == 0) {
		list->scores[0] = score;
		list->numScores++;
	} else if (num == MAX_SCORES) {
		while (pos < num && val <= list->scores[pos].points) pos++;
		if (pos == lastpos) {
			list->scores[pos] = score;
		} else if (pos < lastpos) {
			for (int i=lastpos; i>pos; i--) list->scores[i] = list->scores[i-1];
			list->scores[pos] = score;
		}
	} else {
		while (pos < num && val <= list->scores[pos].points) pos++;
		for (int i=num; i>pos; i--) list->scores[i] = list->scores[i-1];
		list->scores[pos] = score;
		list->numScores++;
	}
	return pos;
}

// for testing:
void CScore::PrintScorelist(const std::string& group, const std::string& course) const {
	const TScoreList *list = &Scorelist.at(group).at(course);

	if (list->numScores < 1) {
		PrintStr("no entries in this score list");
	} else {
		for (int i=0; i<list->numScores; i++) {
			std::string line = "player: " + list->scores[i].player;
			line += " points: " + Int_StrN(list->scores[i].points);
			line += " herrings: " + Int_StrN(list->scores[i].herrings);
			line += " time: " + Float_StrN(list->scores[i].time, 2);
			PrintString(line);
		}
	}
}

const TScoreList* CScore::GetScorelist(const std::string& group, const std::string& course) const {
	try {
		return &Scorelist.at(group).at(course);
	} catch (...) {
		return nullptr;
	}
}

bool CScore::SaveHighScore() const {
	CSPList splist;

	for (std::unordered_map<std::string, std::unordered_map<std::string, TScoreList>>::const_iterator i = Scorelist.cbegin(); i != Scorelist.cend(); ++i) {
		for (std::unordered_map<std::string, TScoreList>::const_iterator j = i->second.cbegin(); j != i->second.cend(); ++j) {
			const TScoreList *list = &j->second;

			int num = list->numScores;
			if (num > 0) {
				for (int sc = 0; sc<num; sc++) {
					const TScore& score = list->scores[sc];
					std::string line = "*[group] " + i->first;
					line += " [course] " + j->first;
					line += " [plyr] " + score.player;
					line += " [pts] " + Int_StrN(score.points);
					line += " [herr] " + Int_StrN(score.herrings);
					line += " [time] " + Float_StrN(score.time, 1);
					splist.Add(line);
				}
			}
		}
	}

	if (!splist.Save(param.config_dir, "highscore")) {
		Message("could not save highscore list");
		return false;
	}
	return true;
}

bool CScore::LoadHighScore() {
	CSPList list;

	if (!list.Load(param.config_dir, "highscore")) {
		Message("could not load highscore list");
		return false;
	}

	Scorelist.clear(); // A loaded save replaces scores instead of duplicating them.

	for (CSPList::const_iterator line = list.cbegin(); line != list.cend(); ++line) {
		std::string group = SPStrN(*line, "group", "default");
		std::string course = SPStrN(*line, "course", "unknown");
		try {
			AddScore(group, course, TScore(
			             SPStrN(*line, "plyr", "unknown"),
			             SPIntN(*line, "pts", 0),
			             SPIntN(*line, "herr", 0),
			             SPFloatN(*line, "time", 0)));
		} catch (std::exception&)
		{ }
	}
	return true;
}

int CScore::CalcRaceResult() {
	g_game.race_result = -1;
	if (g_game.game_type == CUPRACING) {
		if (g_game.time <= g_game.race->time.x &&
		        g_game.herring >= g_game.race->herrings.x) g_game.race_result = 0;
		if (g_game.time <= g_game.race->time.y &&
		        g_game.herring >= g_game.race->herrings.y) g_game.race_result = 1;
		if (g_game.time <= g_game.race->time.z &&
		        g_game.herring >= g_game.race->herrings.z) g_game.race_result = 2;
	}

	int herringpt = g_game.herring * 10;
	float timept = Course.GetDimensions().y - (g_game.time * 10);
	g_game.score = (int)(herringpt + timept);
	if (g_game.score < 0) g_game.score = 0;

	return AddScore(Course.currentCourseList->name, g_game.course->dir, TScore(g_game.player->name, g_game.score, g_game.herring, g_game.time));
}

// --------------------------------------------------------------------
//				score screen
// --------------------------------------------------------------------

// PSP: two rows choose the course set and the course, the scores of that
// course stand under them. Up and down choose the row, left and right
// change it, Circle or Cross goes back.
enum { ROW_GROUP, ROW_COURSE, ROW_COUNT };
static CCourseList *CourseList;
static int cursor_row = ROW_COURSE;
static int sel_group = 0, sel_course = 0;

static int wrap(int value, int count) {
	return count > 0 ? (value % count + count) % count : 0;
}

void CScore::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release) return;
	const int step = PspUI::ListKey(key, cursor_row, ROW_COUNT);
	if (step) {
		if (cursor_row == ROW_GROUP) {
			sel_group = wrap(sel_group + step, (int)Course.CourseLists.size());
			CourseList = Course.getGroup((std::size_t)sel_group);
			sel_course = 0;
		} else sel_course = wrap(sel_course + step, (int)CourseList->size());
		return;
	}
	if (key == sf::Keyboard::Escape || key == sf::Keyboard::Return)
		State::manager.RequestEnterState(*State::manager.PreviousState());
}

void CScore::Mouse(int button, int state, int x, int y) {}
void CScore::Motion(int x, int y) {}

void CScore::Enter() {
	Winsys.ShowCursor(false);
	Music.Play(param.menu_music, true);
	ResetGUI();

	CourseList = &Course.CourseLists["default"];
	for (int i = 0; i < (int)Course.CourseLists.size(); ++i)
		if (Course.getGroup((std::size_t)i) == CourseList) sel_group = i;
	sel_course = wrap(sel_course, (int)CourseList->size());
	cursor_row = ROW_COURSE;
}

void CScore::Loop(float time_step) {
	ScopedRenderMode rm(GUI);
	Winsys.clear();

	if (param.ui_snow) {
		update_ui_snow(time_step);
		draw_ui_snow();
	}

	DrawGUIBackground(Winsys.scale);

	std::vector<PspUI::Row> rows(ROW_COUNT);
	rows[ROW_GROUP] = {Trans.Text(TXT_COURSE_SET), CourseList->name, Course.CourseLists.size() > 1};
	rows[ROW_COURSE] = {Trans.Text(TXT_COURSE), (*CourseList)[sel_course].name, CourseList->size() > 1};
	PspUI::OptionList(127, 100, 600, rows, cursor_row);

	const TScoreList *list = Score.GetScorelist(CourseList->name, (*CourseList)[sel_course].dir);

	// The scores: place, points, player, herring and time, in columns.
	const int left = 137, top = 100 + ROW_COUNT * PspUI::RowHeight + 14, line = 30;
	FT.SetColor(colWhite);
	FT.SetSize(22);
	if (list != nullptr && list->numScores > 0) {
		for (int i=0; i<std::min(MAX_SCORES, list->numScores) && top + (i + 1) * line < 428; i++) {
			int y = top + i*line;
			FT.DrawString(left, y, Trans.Text(99+i));
			FT.DrawString(left + 60, y, Int_StrN(list->scores[i].points));
			FT.DrawString(left + 140, y, list->scores[i].player);
			FT.DrawString(left + 340, y,
			              Int_StrN(list->scores[i].herrings) + "  " + Trans.Text(97));
			FT.DrawString(left + 460, y,
			              Float_StrN(list->scores[i].time, 1) + "  " + Trans.Text(98));
		}
	} else
		FT.DrawString(CENTER, top + 40, Trans.Text(63));

	PspUI::Hint(44, 432, PspUI::Circle, Trans.Text(8));
	PspUI::Hint(560, 432, PspUI::Dpad, Trans.Text(TXT_CHOOSE_CHANGE));

	Winsys.SwapBuffers();
}
