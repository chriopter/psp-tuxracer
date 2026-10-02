/* --------------------------------------------------------------------
EXTREME TUXRACER

Copyright (C) 2004-2005 Volker Stroebel (Planetpenguin Racer)
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

#ifndef TRANSLATION_H
#define TRANSLATION_H

#include "bh.h"
#include <vector>

#define NUM_COMMON_TEXTS 176
// PSP menus: the rows and key hints of the list menus, from 111 on.
enum PspText { TXT_COURSE = 111, TXT_COURSE_SET, TXT_PLAYER, TXT_CHARACTER, TXT_EVENT, TXT_CUP, TXT_NAME,
	TXT_AVATAR, TXT_ADD_PLAYER, TXT_LANGUAGE, TXT_SAVED_DATA, TXT_SAVE_PROGRESS, TXT_LOAD_PROGRESS,
	TXT_RACE, TXT_CONTINUE, TXT_QUIT, TXT_CHOOSE_CHANGE, TXT_SAVE, TXT_CANCEL, TXT_NEW_PLAYER,
	TXT_EDIT_NAME, TXT_ADD, TXT_NEXT, TXT_RANDOMIZE, TXT_LOAD, TXT_CHOOSE, TXT_UP_DOWN, TXT_SAVE_CONTENTS,
	TXT_VS_PENGUINS, TXT_PENGUINS, TXT_PSP_EXCLUSIVE, TXT_PLACE, TXT_VS_SHORT, TXT_ON, TXT_OFF,
	// The controls picture, the pause menu and the questions before leaving.
	TXT_GUIDE_TITLE, TXT_BRAKE, TXT_PADDLE, TXT_STEER, TXT_DPAD, TXT_ANALOG, TXT_RESET, TXT_TRIANGLE,
	TXT_TRICK, TXT_TRICK_KEYS, TXT_BACK, TXT_BACK_KEYS, TXT_JUMP, TXT_JUMP_KEYS, TXT_START_PAUSE, TXT_JUMP_HOW,
	TXT_START_MENU, TXT_START_BACK, TXT_PAUSED, TXT_RESUME, TXT_CONTROLS, TXT_END_RACE, TXT_SELECT,
	TXT_AUTOSAVE_PAUSED, TXT_CONFIRM, TXT_END_RACE_ASK, TXT_END_RACE_NOTE, TXT_QUIT_ASK, TXT_QUIT_NOTE, TXT_NONE };

/* --------------------------------------------------------------------
Name convention:
"lang" means the short identifier, e.g. "en_GB"
"language" means the language name, e.g. "English"
---------------------------------------------------------------------*/

struct TLang {
	std::string lang;
	sf::String language;
};

class CTranslation {
private:
	sf::String texts[NUM_COMMON_TEXTS];
public:
	std::vector<TLang> languages;

	void LoadLanguages();
	const sf::String& GetLanguage(std::size_t idx) const;
	void SetDefaultTranslations();
	const sf::String& Text(std::size_t idx) const;
	void LoadTranslations(std::size_t langidx);
	void ChangeLanguage(std::size_t langidx);
	static std::string GetSystemDefaultLang();
	std::size_t GetSystemDefaultLangIdx() const;
	std::size_t GetLangIdx(const std::string& lang) const;
};

extern CTranslation Trans;


#endif
