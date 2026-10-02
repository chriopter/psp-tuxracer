// PSP port, 2026-10-01: computer penguins to race against, in practice and
// in events. A PSP exclusive; the desktop game has only the clock.
//
// They are not simulated as the player is. Each follows the course downhill
// and picks the lower, tree-free side a few metres ahead. Its speed is the
// player's own recent best, a little less for most of them, eased off when
// it leads and pressed on when it trails: a clean run wins, a fall costs
// places, and the race stays close. They do not jump or fall, but they are
// solid: to each other and to the player.
#ifndef OPPONENTS_H
#define OPPONENTS_H

class CControl;

namespace Opponents {
enum { MAX = 5 };
extern bool enabled;            // the tick in the race and event selection
extern int count;               // how many, 1..MAX, when enabled
void Start(const CControl* player);   // at the start of a race, beside the player
void Update(float time_step, CControl* player);   // moves them; a collision moves the player too
void Draw();                    // each with the shape of a character of its own
// The player's place among all racers, 1 for the lead; 0 without opponents.
// Fixed from the moment the player crosses the line.
int Place(const CControl* player);
int Count();
// Keys for a player that drives itself, for filmed runs: 1 left, 2 right,
// 4 paddle, 8 reset to the course.
int Autopilot(const CControl* player);
}

#endif
