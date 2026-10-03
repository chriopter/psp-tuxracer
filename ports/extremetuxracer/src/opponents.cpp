// PSP port, 2026-10-01. See opponents.h.

#ifdef HAVE_CONFIG_H
#include <etr_config.h>
#endif

#include "opponents.h"
#include "course.h"
#include "game_ctrl.h"
#include "physics.h"
#include "audio.h"
#include "tux.h"
#include "view.h"
#include "ogl.h"
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

bool PspFixedStep();

const std::vector<unsigned>& ObjectsNear(bool trees, float z, float reach);

static std::vector<std::string> race_log;
void PspRaceLogFlush() {
	if (race_log.empty()) return;
	if (FILE* log = std::fopen("config/race-log.txt", "w")) {
		for (const auto& line : race_log) std::fprintf(log, "%s\n", line.c_str());
		std::fclose(log);
	}
	race_log.clear();
}

namespace Opponents {

bool enabled = false;
int count = 3;

struct Racer {
	float x, z, speed;
	float skill;        // how well this one drives: 1 is the player's own usual run
	float drive;        // the speed the slope gives it, before skill and level
	float cosine;       // of the slope under it, as last looked at
	float waited;       // time since its drive was last worked out
	float half, front, back;    // how far its body reaches: to a side, ahead, behind
	float heading;      // sideways speed, eased, for the line and the lean
	float goal;         // the x it is making for
	CCharShape* shape;  // one of the game's characters
	bool finished;
	float shown;        // its speed as the last frame left it
	const TCharacter* who;  // for its name in the result
	float time;         // on the race's clock when it crossed the line
};
static Racer racers[MAX];
// Each penguin's own, kept from one look for its line to the next: the
// trees it could run into until then are among them.
static std::vector<unsigned> trees_of[MAX];
static int racing = 0;
// What the player makes of a slope, as a share of what the forces alone
// would give: learned from every race of this session, slowly, and the
// measure of the field. The first race starts from a fair guess.
static float level = 0.7f;
static float shadow = 0;            // the forces' speed along the player's own way
static int final_place = 0;
static float player_x = 0, player_z = 0;
static bool touching = false, bumped = false;   // the player against a penguin: now, and last frame
static unsigned tick = 0;           // the line is looked for every eighth frame, in turn
static float race_clock = 0;        // runs on when the player's own has stopped at the line
static float player_time = 0;       // the player's time, once through
static float me_half = 0.3f, me_front = 0.55f, me_back = 0.5f;     // the player's own body
static bool show_boxes = false;     // config/debug-collision: the boxes that collide, drawn
static bool trace_boxes = false;    // config/trace: the bodies' sizes in the log
static bool approach_test = false;  // config/debug-collision-test: a penguin brought up to the player step by step

int Count() { return racing; }
int Chosen() { return enabled ? (count < 1 ? 1 : count > MAX ? MAX : count) : 0; }
void Choose(int n) {
	enabled = n > 0;
	if (n > 0) count = n > MAX ? MAX : n;
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void Start(const CControl* player) {
	tick = 0;
	for (auto& list : trees_of) list.clear();
	race_clock = 0;
	player_time = 0;
	shadow = std::max(3.f, (float)TVector3d(player->cvel).Length());
	show_boxes = access("config/debug-collision", F_OK) == 0;
	trace_boxes = access("config/trace", F_OK) == 0;
	approach_test = access("config/debug-collision-test", F_OK) == 0;
	if (g_game.character && g_game.character->shape) {
		CCharShape* own = g_game.character->shape;
		own->Measure();
		me_half = own->extHalfWidth; me_front = own->extFront; me_back = own->extBack;
		if (trace_boxes) std::fprintf(stderr, "BOX player half %.3f front %.3f back %.3f\n", me_half, me_front, me_back);
	}
	racing = enabled ? (count < 1 ? 1 : count > MAX ? MAX : count) : 0;
	final_place = 0;
	if (!racing) return;

	// A character each, drawn at random; the player's own last, so that
	// with few opponents they are the others.
	std::vector<const TCharacter*> shapes;
	// Their shapes are read now, if this is the first race against them.
	for (std::size_t i = 0; i < Char.CharList.size(); ++i) Char.Ensure(Char.CharList[i]);
	for (std::size_t i = 0; i < Char.CharList.size(); ++i)
		if (Char.CharList[i].shape && &Char.CharList[i] != g_game.character)
			shapes.push_back(&Char.CharList[i]);
	for (std::size_t i = shapes.size(); i > 1; --i)
		std::swap(shapes[i - 1], shapes[std::rand() % i]);
	if (g_game.character && g_game.character->shape) shapes.push_back(g_game.character);

	// The field, the best first, so that a single opponent is the one to
	// beat: one a little better than the player usually is, one their
	// equal, the rest behind. Each has its day, a little up or down. Who
	// is which is drawn.
	float skills[MAX] = {1.04f, 1.f, 0.97f, 0.94f, 0.9f};
	for (int i = 0; i < MAX; ++i) skills[i] += (std::rand() % 31 - 15) * 0.001f;
	for (int i = racing; i > 1; --i) std::swap(skills[i - 1], skills[std::rand() % i]);

	const float width = Course.GetDimensions().x;
	for (int i = 0; i < racing; ++i) {
		// Left and right of the player in turn, on the line.
		const float side = (i % 2 ? 1.f : -1.f) * 1.5f * (i / 2 + 1);
		const float x = clampf((float)player->cpos.x + side, 2.f, width - 2.f);
		const TCharacter* who = shapes.empty() ? nullptr : shapes[i % shapes.size()];
		if (who && who->shape) who->shape->Measure();
		const float half = who && who->shape ? who->shape->extHalfWidth : 0.3f;
		const float front = who && who->shape ? who->shape->extFront : 0.55f, back = who && who->shape ? who->shape->extBack : 0.5f;
		if (trace_boxes) std::fprintf(stderr, "BOX racer %d half %.3f front %.3f back %.3f\n", i, half, front, back);
		racers[i] = {x, (float)player->cpos.z, shadow, skills[i], shadow, 1.f, 0.f, half, front, back, 0.f, x,
		             who ? who->shape : nullptr, false, 0.f, who, 0.f};
	}
}

// The trees between two places down the course, asked for once before a
// line is looked for: with the trees where the course has them, asking for
// every point of every line was two milliseconds of each frame.
static std::vector<unsigned> ahead;
static void gather_trees(float z_from, float z_to) {
	const std::vector<unsigned>& found = ObjectsNear(true, (z_from + z_to) * 0.5f, std::fabs(z_from - z_to) * 0.5f + 2.5f);
	ahead.assign(found.begin(), found.end());
}

static bool tree_at(float x, float z) {
	for (unsigned i : ahead) {
		const TCollidable& tree = Course.CollArr[i];
		const float dz = tree.pt.z - z, deep = 2.5f + tree.diam * 0.5f;
		if (dz < -deep || dz > deep) continue;
		const float dx = tree.pt.x - x;
		const float reach = tree.diam * 0.5f + 1.2f;
		if (dx > -reach && dx < reach) return true;
	}
	return false;
}

// What the ground at a place does to speed: 1 on snow, less on what brakes.
static float ground_pace(float x, float z) {
	const int terrain = Course.GetTerrainIdx(x, z, 0.5f);
	if (terrain < 0) return 1.f;
	return clampf(1.25f - Course.TerrList[terrain].friction * 0.7f, 0.6f, 1.05f);
}

// How good a line is that ends at x, a little way on: low ground is fast
// ground; a tree there or on the way, rock, another racer or the edge of
// the course is not.
static float line_cost(const Racer& self, float x, float z, float width) {
	if (x < 1.5f || x > width - 1.5f) return 1e6f;
	float cost = Course.FindYCoord(x, z);
	// Trees along the way there, the nearer the worse, and a little for
	// those the line would run into beyond.
	for (int k = 1; k <= 7; ++k) {
		const float t = k * 0.25f;
		if (tree_at(k <= 4 ? self.x + (x - self.x) * t : x, self.z + (z - self.z) * t))
			cost += k <= 4 ? 8.f - k : 2.f;
	}
	cost += (1.f - ground_pace(x, z)) * 8.f;
	for (int i = 0; i < racing; ++i) {
		const Racer& other = racers[i];
		if (&other == &self) continue;
		if (std::fabs(other.z - self.z) < 3.f && std::fabs(other.x - x) < 1.2f) cost += 1.5f;
	}
	// The player is in the way like any other, when just ahead.
	if (player_z < self.z + 1.f && player_z > self.z - 8.f && std::fabs(player_x - x) < 1.2f) cost += 1.5f;
	return cost;
}

// The air's braking as the player's physics has it (CControl::CalcAirForce),
// as a deceleration of a penguin's twenty kilograms.
static float air_drag_exact(float speed) {
	static const float log_re[] = {-1, 0, 1, 2, 3, 4, 5, 6};
	static const float log_drag[] = {2.25f, 1.35f, 0.6f, 0, -0.35f, -0.45f, -0.33f, -0.9f};
	const float re = clampf(std::log10(34600.f * std::max(speed, 0.01f)), -1.f, 6.f);
	int k = (int)re + 1;
	if (k > 6) k = 6;
	const float coefficient = std::pow(10.f, log_drag[k] + (log_drag[k + 1] - log_drag[k]) * (re - log_re[k]));
	return 0.104f * coefficient * speed * speed / 20.f;
}
// From a table by half metres a second: a logarithm and a power for every
// penguin in every frame were two milliseconds of each frame on the PSP.
static float air_drag(float speed) {
	enum { STEPS = 96 };
	static float table[STEPS + 1];
	static bool made = false;
	if (!made) {
		for (int i = 0; i <= STEPS; ++i) table[i] = air_drag_exact(i * 0.5f);
		made = true;
	}
	const float at = clampf(speed * 2.f, 0.f, STEPS - 0.001f);
	const int i = (int)at;
	return table[i] + (table[i + 1] - table[i]) * (at - i);
}

// A second of driving by the forces the player drives by, along the slope
// under a place: gravity, the friction of the ground, the air, and the
// flippers when slow. Returns the new speed along the slope; *cosine is
// what of it goes down the course.
static float drive_step(float x, float z, float speed, float dt, float* cosine) {
	const float drop = Course.FindYCoord(x, z) - Course.FindYCoord(x, z - 2.f);
	const float length = std::sqrt(4.f + drop * drop);
	const float sine = drop / length;
	*cosine = 2.f / length;
	const int terrain = Course.GetTerrainIdx(x, z, 0.5f);
	const float friction = terrain < 0 ? 0.35f : Course.TerrList[terrain].friction;
	float push = 9.81f * (sine - friction * *cosine) - air_drag(speed);
	if (speed < 15.f) push += 6.1f * (1.f - speed / 16.67f) * std::min(1.f, friction / 0.35f);
	return clampf(speed + push * dt, 3.f, 45.f);
}

// Racers are solid to each other. Each is taken as a box on the snow as
// large as its own body is drawn (CCharShape::Measure; the characters
// differ), a little smaller for a body being round: so that two stop where
// they are seen to touch, with neither a gap nor one inside the other
// (2026-10-04; one box of 0.6 x 1.05 m for all had both). Two side by side
// are put apart at once; of two in line the one behind cannot go faster
// than the one ahead and passes some of its speed on.
static const float WIDE = 0.6f, LONG = 1.05f;     // the box of a body not measured
static const float FIT_SIDE = 0.82f, FIT_LONG = 0.9f;
struct Body { float x, z, half, front, back; };

// How far two bodies are into each other, across and along the course; false if not at all.
static bool into(const Body& a, const Body& b, float* across, float* along) {
	*across = (a.half + b.half) * FIT_SIDE - std::fabs(b.x - a.x);
	if (*across <= 0) return false;
	// z falls down the course: a body reaches from z - front to z + back
	const float ahead = std::max(a.z - a.front * FIT_LONG, b.z - b.front * FIT_LONG);
	const float behind = std::min(a.z + a.back * FIT_LONG, b.z + b.back * FIT_LONG);
	*along = behind - ahead;
	return *along > 0;
}

static void collide(float dt, CControl* player, float width) {
	const Body me = {(float)player->cpos.x, (float)player->cpos.z, me_half, me_front, me_back};
	for (int i = 0; i < racing; ++i) {
		Racer& a = racers[i];
		for (int j = i + 1; j < racing; ++j) {
			Racer& b = racers[j];
			float across, along;
			if (!into({a.x, a.z, a.half, a.front, a.back}, {b.x, b.z, b.half, b.front, b.back}, &across, &along)) continue;
			const float dx = b.x - a.x;
			const float side = dx > 0 || (dx == 0 && ((i + j) & 1)) ? 1.f : -1.f;
			Racer& rear = b.z > a.z ? b : a;        // z falls down the course
			Racer& front = b.z > a.z ? a : b;
			if (across <= along) {                  // side by side: apart, at once
				a.x -= side * across * 0.5f;
				b.x += side * across * 0.5f;
			} else {                                // in line: the one behind stays behind
				rear.z += along;
				a.x -= side * std::min(across, 2.f * dt) * 0.5f;
				b.x += side * std::min(across, 2.f * dt) * 0.5f;
				if (rear.speed > front.speed) {
					const float closing = rear.speed - front.speed;
					rear.speed -= closing * 0.7f;
					front.speed += closing * 0.2f;
				}
			}
		}

		// And to the player, who feels it: shoved aside, braked when
		// running into one, pushed on when run into. Not in the air above.
		float across, along;
		if (into(me, {a.x, a.z, a.half, a.front, a.back}, &across, &along) &&
		        player->cpos.y - Course.FindYCoord(a.x, a.z) < 1.2) {
			const float dx = a.x - me.x;
			const float side = dx > 0 || (dx == 0 && (i & 1)) ? 1.f : -1.f;
			const float player_down = -(float)player->cvel.z;       // speed down the course
			if (across <= along) {                  // side by side: it gives way at once, the player is shoved
				a.x += side * across;
				a.heading += side * 10.f * dt;
				player->cvel.x -= side * 10.f * dt;
			} else if (a.z < me.z) {                // the player runs into it from behind
				a.z -= along;
				a.x += side * std::min(across, 2.f * dt);
				if (player_down > a.speed) {
					const float closing = player_down - a.speed;
					player->cvel.z += closing * std::min(0.5f, 15.f * dt);
					a.speed += closing * std::min(0.3f, 9.f * dt);
					if (closing > 3.f && !bumped) Sound.Play("tree_hit", 0);
				}
			} else {                                // it runs into the player from behind
				a.z += along;
				a.x += side * std::min(across, 2.f * dt);
				if (a.speed > player_down) {
					const float closing = a.speed - player_down;
					a.speed -= closing * 0.7f;
					player->cvel.z -= closing * 0.2f;
				}
			}
			touching = true;
		}
		a.x = clampf(a.x, 1.5f, width - 1.5f);
	}
}

void Update(float dt, CControl* player) {
	if (!racing) return;
	player_x = (float)player->cpos.x;
	player_z = (float)player->cpos.z;
	if (dt > 0.1f) dt = 0.1f;
	const float width = Course.GetDimensions().x;
	const float finish = -Course.GetPlayDimensions().y;
	// Down the course, as the penguins are moved: not along the slope.
	const float player_speed = std::max(0.f, -(float)player->cvel.z);
	// The player against the forces alone, on their own way down.
	static float player_cosine = 1.f, shadow_waited = 0;
	shadow_waited += dt;
	if (tick % 8 == 7) {
		shadow = drive_step(player_x, player_z, shadow, shadow_waited, &player_cosine);
		shadow_waited = 0;
	}
	if (!g_game.finish)
		if (shadow > 12.f)
			level += (clampf(player_speed / (shadow * player_cosine), 0.3f, 1.2f) - level) * clampf(dt / 25.f, 0.f, 1.f);

	++tick;
	race_clock += dt;
	if (g_game.finish && player_time == 0) player_time = race_clock;
	for (int i = 0; i < racing; ++i) {
		Racer& r = racers[i];
		if (r.finished) {
			r.speed *= 0.95f;
			r.z -= r.speed * dt;
			continue;
		}
		// What it ran into since the last frame slowed its drive too.
		if (r.shown > 0.1f && r.speed < r.shown) r.drive *= std::max(r.speed, 0.f) / r.shown;
		// The slope is looked at every eighth frame, each penguin in its
		// turn, for the time since: the ground under it changes slowly.
		r.waited += dt;
		if ((tick + i) % 8 == 1) {
			r.drive = drive_step(r.x, r.z, r.drive, r.waited, &r.cosine);
			r.waited = 0;
		}
		const float cosine = r.cosine;
		// The band, far from the player only: well ahead it eases off, so
		// that a fall is not the end of the race; well behind it presses
		// on. Within fifteen metres the better drive wins.
		const float lead = (float)player->cpos.z - r.z;        // positive: it is ahead
		const float band = lead > 15.f ? clampf(1.f - (lead - 15.f) * 0.004f, 0.6f, 1.f)
		                 : lead < -15.f ? clampf(1.f - (lead + 15.f) * 0.004f, 1.f, 1.1f) : 1.f;
		// Slow, at the start and on the flat, anyone is as quick as the
		// forces allow; what the player loses to them shows at speed.
		const float share = level + (1.f - level) * clampf((18.f - r.drive) / 10.f, 0.f, 1.f);
		const float advance = r.drive * cosine * share * r.skill * band;
		r.speed = r.shown = advance;

		// The line: of five places across, the cheapest about a second on.
		const float look = r.z - (6.f + r.speed * 0.7f);
		if ((tick + i) % 8 == 5) {
			gather_trees(r.z + 3.f, look);
			trees_of[i] = ahead;
			float best = r.x, best_cost = line_cost(r, r.x, look, width) - 0.3f;   // staying put is worth a little
			for (float step : {-3.f, -1.5f, 1.5f, 3.f}) {
				const float c = line_cost(r, r.x + step, look, width);
				if (c < best_cost) { best_cost = c; best = r.x + step; }
			}
			r.goal = best;
		}
		const float sideways = clampf((r.goal - r.x) * 1.5f, -5.f, 5.f);
		r.heading += (sideways - r.heading) * clampf(dt * 4.f, 0.f, 1.f);
		r.x = clampf(r.x + r.heading * dt, 1.5f, width - 1.5f);
		r.z -= advance * dt;
		// A tree it did not get round stops it as it stops the player:
		// most of its speed is gone, and it is put beside the trunk.
		for (unsigned t : trees_of[i]) {
			const float dx = r.x - Course.CollArr[t].pt.x;
			const float trunk = Course.CollArr[t].diam * 0.5f + 0.3f;
			if (std::fabs(dx) < trunk && std::fabs(r.z - Course.CollArr[t].pt.z) < 0.5f) {
				r.x = Course.CollArr[t].pt.x + (dx < 0 ? -trunk : trunk);
				if (r.speed > 4.f) r.speed = 4.f;
			}
		}
		if (r.z <= finish) {
			r.finished = true;
			r.time = race_clock - (finish - r.z) / std::max(advance, 1.f);
		}
	}
	// A filmed run keeps a note of the race, twice a second: the player's
	// speed, the level and each penguin's lead in metres and its speed.
	// Kept in memory and written when the race is over (PspRaceLogFlush):
	// a file opened twice a second cost the frames it was to be about.
	if (PspFixedStep() && tick % 30 == 0 && race_log.size() < 1500) {
		char line[160];
		int n = std::snprintf(line, sizeof line, "%u %.1f %.2f", tick, player_speed, level);
		for (int i = 0; i < racing && n < (int)sizeof line - 16; ++i)
			n += std::snprintf(line + n, sizeof line - n, " %.1f/%.1f", (float)player->cpos.z - racers[i].z, racers[i].speed);
		race_log.push_back(line);
	}
	bumped = touching;
	touching = false;
	// Debug: the first penguin is put beside the player, nearer by a step
	// every three quarters of a second, then ahead of them likewise; the
	// collision is left to stop it. What the pictures then show is how
	// close two bodies come.
	if (approach_test && racing) {
		Racer& r = racers[0];
		const int step = (int)(race_clock / 0.75f);
		for (int k = 1; k < racing; ++k) racers[k].x = 2.f, racers[k].z = player_z + 30.f + 3.f * k;   // the others out of the way
		// from the right down to 0.3 m, from the left likewise, then from ahead down to 0.6 m
		if (step < 8) { r.x = player_x + std::max(0.3f, 1.6f - 0.2f * step); r.z = player_z; }
		else if (step < 16) { r.x = player_x - std::max(0.3f, 1.6f - 0.2f * (step - 8)); r.z = player_z; }
		else { r.x = player_x; r.z = player_z - std::max(0.6f, 3.f - 0.3f * (step - 16)); }
		r.speed = r.shown = player_speed; r.heading = 0;
	}
	if (!g_game.finish) collide(dt, player, width);
	if (approach_test && racing && tick % 15 == 0)
		std::fprintf(stderr, "APPROACH %.2f dx %.3f dz %.3f\n", race_clock, racers[0].x - player_x, racers[0].z - player_z);
}

int Autopilot(const CControl* player) {
	// The player's penguin driven as the others pick their line: for the
	// capture runs, so that a film of a race is not one of a penguin in a
	// tree. Not used in a measurement, whose input stays as it always was.
	Racer self = {};
	self.x = (float)player->cpos.x;
	self.z = (float)player->cpos.z;
	const float speed = (float)TVector3d(player->cvel).Length();
	const float width = Course.GetDimensions().x;
	const float look = self.z - (8.f + speed * 0.8f);
	const float save_x = player_x, save_z = player_z;
	player_z = 1e9f;                    // not in its own way
	gather_trees(self.z, look);
	float best = self.x, best_cost = line_cost(self, self.x, look, width) - 0.3f;
	for (float step : {-4.f, -2.f, 2.f, 4.f}) {
		const float c = line_cost(self, self.x + step, look, width);
		if (c < best_cost) { best_cost = c; best = self.x + step; }
	}
	player_x = save_x;
	player_z = save_z;
	const float wanted = (best - self.x) * 1.5f;        // sideways speed to get there
	int keys = speed < 14.f ? 4 : 0;
	// Stuck in the trees for three seconds: back onto the course, as a
	// player would with Triangle.
	static int stuck = 0;
	stuck = speed < 2.f ? stuck + 1 : 0;
	if (stuck > 180) { stuck = 0; return 8; }
	if ((float)player->cvel.x < wanted - 0.5f) keys |= 2;
	else if ((float)player->cvel.x > wanted + 0.5f) keys |= 1;
	return keys;
}

int Place(const CControl* player) {
	if (!racing) return 0;
	if (final_place) return final_place;
	int place = 1;
	for (int i = 0; i < racing; ++i)
		if (racers[i].z < player->cpos.z) ++place;
	if (g_game.finish) final_place = place;
	return place;
}

int Results(const CControl* player, Result* out) {
	if (!racing) return 0;
	const float finish = -Course.GetPlayDimensions().y;
	int n = 0;
	out[n++] = {nullptr, g_game.time, true, false};
	for (int i = 0; i < racing; ++i) {
		const Racer& r = racers[i];
		// One still on the course when the result is shown: its time as
		// it would come in at the speed it has. Never before the player.
		float time = r.time;
		if (!r.finished) time = std::max(race_clock + (r.z - finish) / std::max(r.speed, 5.f), player_time + 0.01f);
		// The player's time is on the game's clock, the penguins' on the race's.
		time += g_game.time - (player_time > 0 ? player_time : race_clock);
		out[n++] = {r.who ? &r.who->name : nullptr, time, false, !r.finished};
	}
	for (int i = 1; i < n; ++i)
		for (int j = i; j > 0 && out[j].time < out[j - 1].time; --j) std::swap(out[j], out[j - 1]);
	(void)player;
	return n;
}

static void draw_box(float x, float z, float half, float front, float back) {
	const float xs[5] = {-half * FIT_SIDE, half * FIT_SIDE, half * FIT_SIDE, -half * FIT_SIDE, -half * FIT_SIDE};
	const float zs[5] = {-front * FIT_LONG, -front * FIT_LONG, back * FIT_LONG, back * FIT_LONG, -front * FIT_LONG};
	glBegin(GL_LINE_STRIP);
	for (int k = 0; k < 5; ++k) glVertex3f(x + xs[k], Course.FindYCoord(x + xs[k], z + zs[k]) + 0.25f, z + zs[k]);
	glEnd();
}

void Draw() {
	if (show_boxes && racing) {
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_LIGHTING);
		glColor4f(1.f, 0.f, 0.f, 1.f);
		for (int i = 0; i < racing; ++i) draw_box(racers[i].x, racers[i].z, racers[i].half, racers[i].front, racers[i].back);
		glColor4f(1.f, 1.f, 0.f, 1.f);
		draw_box(player_x, player_z, me_half, me_front, me_back);
		glEnable(GL_LIGHTING);
		glEnable(GL_TEXTURE_2D);
	}
	for (int i = 0; i < racing; ++i) {
		const Racer& r = racers[i];
		if (!r.shape) continue;
		const float y = Course.FindYCoord(r.x, r.z);
		const TVector3d pos(r.x, y + TUX_Y_CORR, r.z);
		if (clip_aabb_to_view_frustum(pos + TVector3d(-0.8, -0.8, -0.8), pos + TVector3d(0.8, 0.8, 0.8)) == NotVisible)
			continue;
		// Belly on the snow, head down the hill, turned into its line:
		// the same frame of axes the player's orientation is built from.
		const TVector3d normal = Course.FindCourseNormal(r.x, r.z);
		TVector3d forward(r.heading, 0, r.speed > 1.f ? -r.speed : -1.f);
		forward = ProjectToPlane(normal, forward);
		forward.Norm();
		const TVector3d new_z = -1.0 * normal;
		const TVector3d new_x = CrossProduct(forward, new_z);
		TMatrix<4, 4> axes(new_x, forward, new_z);
		TMatrix<4, 4> place;
		place.SetTranslationMatrix(pos.x, pos.y, pos.z);
		r.shape->DrawBaked(place * axes);
	}
}

}
