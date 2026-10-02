// PSP port, 2026-10-01. The first start asks for the language before
// anything else is read: a list of the translations the game has, the
// D-pad's up and down through it, Cross to take one. The answer is kept in
// the options file, so the question comes once; Configuration changes it
// later.

#ifdef HAVE_CONFIG_H
#include <etr_config.h>
#endif

#include "language_select.h"
#include "ogl.h"
#include "audio.h"
#include "gui.h"
#include "font.h"
#include "particles.h"
#include "translation.h"
#include "game_config.h"
#include "regist.h"
#include "psp_ui.h"
#include "winsys.h"

CLanguageSelect LanguageSelect;

enum { VISIBLE_ROWS = 6 };
static int cursor_row = 0;
static int first_row = 0;

void CLanguageSelect::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release) return;
	const int count = (int)Trans.languages.size();
	PspUI::ListKey(key, cursor_row, count);
	if (cursor_row < first_row) first_row = cursor_row;
	if (cursor_row >= first_row + VISIBLE_ROWS) first_row = cursor_row - VISIBLE_ROWS + 1;
	if (key == sf::Keyboard::Return) {
		if ((int)param.language != cursor_row) {
			param.language = cursor_row;
			Trans.ChangeLanguage(param.language);
		}
		param.language_chosen = true;
		SaveConfigFile();
		State::manager.RequestEnterState(Regist);
	}
}

void CLanguageSelect::Enter() {
	Winsys.ShowCursor(false);
	ResetGUI();
	const int count = (int)Trans.languages.size();
	cursor_row = (int)param.language < count ? (int)param.language : 0;
	first_row = cursor_row >= VISIBLE_ROWS ? cursor_row - VISIBLE_ROWS + 1 : 0;
	Music.Play(param.menu_music, true);
}

void CLanguageSelect::Loop(float time_step) {
	ScopedRenderMode rm(GUI);
	Winsys.clear();

	if (param.ui_snow) {
		update_ui_snow(time_step);
		draw_ui_snow();
	}

	DrawGUIBackground(Winsys.scale);

	FT.SetSize(24);
	FT.SetColor(colWhite);
	FT.DrawString(CENTER, 96, Trans.Text(TXT_LANGUAGE));

	const int count = (int)Trans.languages.size();
	std::vector<PspUI::Row> rows;
	for (int i = first_row; i < count && i < first_row + VISIBLE_ROWS; ++i)
		rows.push_back({Trans.languages[i].language, "", false});
	PspUI::OptionList(227, 140, 400, rows, cursor_row - first_row);

	PspUI::Hint(44, 432, PspUI::Cross, Trans.Text(TXT_CHOOSE));
	PspUI::Hint(560, 432, PspUI::Dpad, Trans.Text(TXT_UP_DOWN));

	Winsys.SwapBuffers();
}
