// PSP port, 2026-10-01: computer penguins to race against, in practice and
// in events. A PSP exclusive; the desktop game has only the clock.
//
// Each follows the course downhill and picks the lower, tree-free side a
// few metres ahead. Its speed is its own (2026-10-03; it was the player's
// before, and the player always won): the slope, the ground's friction and
// the air act on it as on the player, and the field is of different skill,
// the best a little better than a clean run. Only far from the player is it
// held back or pressed on, so that the race stays one. They do not jump or
// fall, but they are solid: to each other and to the player.
#ifndef OPPONENTS_H
#define OPPONENTS_H

#include <string>

class CControl;

namespace Opponents {
enum { MAX = 5 };
extern bool enabled;            // racing against them at all
extern int count;               // how many, 1..MAX, when enabled
// The one row of the race and event selection: none, or one to MAX.
int Chosen();
void Choose(int n);
void Start(const CControl* player);   // at the start of a race, beside the player
void Update(float time_step, CControl* player);   // moves them; a collision moves the player too
void Draw();                    // each with the shape of a character of its own
// The player's place among all racers, 1 for the lead; 0 without opponents.
// Fixed from the moment the player crosses the line.
int Place(const CControl* player);
int Count();
// The race as it came in, the quickest first, for the result screen: every
// racer's time, the player's among them. Returns how many (Count() + 1), 0
// without opponents.
struct Result {
	const std::string* name;    // null for the player
	float time;
	bool player;
	bool estimated;             // still on the course when the player was through
};
int Results(const CControl* player, Result* out);
// Keys for a player that drives itself, for filmed runs: 1 left, 2 right,
// 4 paddle, 8 reset to the course.
int Autopilot(const CControl* player);
}

#endif
