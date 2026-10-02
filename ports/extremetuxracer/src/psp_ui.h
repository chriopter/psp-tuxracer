#ifndef ETR_PSP_UI_H
#define ETR_PSP_UI_H
#include "bh.h"
#include <vector>
namespace PspUI {
enum Button { Cross, Circle, Square, Triangle, Start, ShoulderL, ShoulderR, Dpad };
void Box(int x, int y, int w, int h, sf::Color color);
void Line(float x, float y, float xx, float yy, sf::Color color);
void Text(int x, int y, const char* text, unsigned size = 22);
void Text(int x, int y, const sf::String& text, unsigned size = 22);
void Icon(int x, int y, Button button);
void Hint(int x, int y, Button button, const char* text);
void Hint(int x, int y, Button button, const sf::String& text);
void Background();
void Controls(int x, int y);
void Confirm(const sf::String& title, const sf::String& detail);

// A PSP-style list of settings: one row under the other, the D-pad's up and
// down walk the rows, left and right change the value of the row the cursor
// is on. A row without a value is something to do, taken with Cross.
struct Row {
	sf::String label;
	sf::String value;
	bool adjustable;
	// The sign at the head of the row: a sheet of two by two pictures, as
	// the game's weather buttons are, and which of the four to show.
	const sf::Texture* icon = nullptr;
	int icon_state = 0;
};
enum { RowHeight = 46 };
void OptionList(int x, int y, int w, const std::vector<Row>& rows, int cursor);
// Up and down move the cursor, wrapping. Returns -1 for left, +1 for right
// on the cursor's row, 0 for every other key.
int ListKey(sf::Keyboard::Key key, int& cursor, int rows);
// "Light: Sunny" as the translations have it, taken apart at the colon.
void SplitLabel(const sf::String& text, sf::String& label, sf::String& value);
}
#endif
