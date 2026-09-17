#ifndef ETR_PSP_UI_H
#define ETR_PSP_UI_H
#include "bh.h"
namespace PspUI {
enum Button { Cross, Circle, Square, Triangle, Start, ShoulderL, ShoulderR, Dpad };
void Box(int x, int y, int w, int h, sf::Color color);
void Text(int x, int y, const char* text, unsigned size = 22);
void Icon(int x, int y, Button button);
void Hint(int x, int y, Button button, const char* text);
void Background();
void Controls(int x, int y);
void Confirm(const char* title, const char* detail);
}
#endif
