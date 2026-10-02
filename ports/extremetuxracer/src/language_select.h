// PSP port, 2026-10-01: the language, asked for once on the first start.
#ifndef LANGUAGE_SELECT_H
#define LANGUAGE_SELECT_H

#include "bh.h"
#include "states.h"

class CLanguageSelect : public State {
	void Enter();
	void Loop(float time_step);
	void Keyb(sf::Keyboard::Key key, bool release, int x, int y);
public:
};

extern CLanguageSelect LanguageSelect;

#endif
