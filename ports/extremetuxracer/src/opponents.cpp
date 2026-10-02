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
#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstdio>

bool PspFixedStep();

const std::vector<unsigned>& ObjectsNear(bool trees, float z, float reach);

namespace Opponents {

bool enabled = false;
int count = 3;

struct Racer {
	float x, z, speed;
	float pace;         // this one's share of the field's speed: some are a little quicker
	float heading;      // sideways speed, eased, for the line and the lean
	float goal;         // the x it is making for
	CCharShape* shape;  // one of the game's characters
	bool finished;
};
static Racer racers[MAX];
static int racing = 0;
static float player_pace = 0;       // the player's speed at their recent best
static int final_place = 0;
static float player_x = 0, player_z = 0;
static bool touching = false, bumped = false;   // the player against a penguin: now, and last frame
static unsigned tick = 0;           // the line is looked for every fourth frame, in turn

int Count() { return racing; }

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void Start(const CControl* player) {
	tick = 0;
	racing = enabled ? (count < 1 ? 1 : count > MAX ? MAX : count) : 0;
	final_place = 0;
	player_pace = std::max(0.f, -(float)player->cvel.z);
	if (!racing) return;

	// A character each, drawn at random; the player's own last, so that
	// with few opponents they are the others.
	std::vector<CCharShape*> shapes;
	// Their shapes are read now, if this is the first race against them.
	for (std::size_t i = 0; i < Char.CharList.size(); ++i) Char.Ensure(Char.CharList[i]);
	for (std::size_t i = 0; i < Char.CharList.size(); ++i)
		if (Char.CharList[i].shape && &Char.CharList[i] != g_game.character)
			shapes.push_back(Char.CharList[i].shape);
	for (std::size_t i = shapes.size(); i > 1; --i)
		std::swap(shapes[i - 1], shapes[std::rand() % i]);
	if (g_game.character && g_game.character->shape) shapes.push_back(g_game.character->shape);

	// The field: one as quick as the player at their best, the rest a
	// touch slower. Who is which is drawn too.
	float paces[MAX] = {1.f, 0.97f, 0.99f, 0.94f, 0.91f};
	for (int i = racing; i > 1; --i) std::swap(paces[i - 1], paces[std::rand() % i]);

	const float width = Course.GetDimensions().x;
	for (int i = 0; i < racing; ++i) {
		// Left and right of the player in turn, on the line.
		const float side = (i % 2 ? 1.f : -1.f) * 1.5f * (i / 2 + 1);
		const float x = clampf((float)player->cpos.x + side, 2.f, width - 2.f);
		racers[i] = {x, (float)player->cpos.z, player_pace, paces[i], 0.f, x,
		             shapes.empty() ? nullptr : shapes[i % shapes.size()], false};
	}
}

static bool tree_at(float x, float z) {
	for (unsigned i : ObjectsNear(true, z, 2.5f)) {
		const float dx = Course.CollArr[i].pt.x - x;
		const float reach = Course.CollArr[i].diam * 0.5f + 1.2f;
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

// Racers are solid to each other. A penguin is taken as a box on the snow,
// WIDE across and LONG down the course. Two that overlap are pushed apart
// sideways; of two in line, the one behind cannot go faster than the one
// ahead, and passes some of its speed on.
static const float WIDE = 0.95f, LONG = 1.5f;

static void collide(float dt, CControl* player, float width) {
	for (int i = 0; i < racing; ++i) {
		Racer& a = racers[i];
		for (int j = i + 1; j < racing; ++j) {
			Racer& b = racers[j];
			const float dx = b.x - a.x, dz = b.z - a.z;
			if (std::fabs(dx) >= WIDE || std::fabs(dz) >= LONG) continue;
			const float side = dx > 0 || (dx == 0 && ((i + j) & 1)) ? 1.f : -1.f;
			const float push = std::min(WIDE - std::fabs(dx), 6.f * dt) * 0.5f;
			a.x -= side * push;
			b.x += side * push;
			Racer& rear = dz < 0 ? a : b;       // z falls down the course
			Racer& front = dz < 0 ? b : a;
			if (rear.speed > front.speed) {
				const float closing = rear.speed - front.speed;
				rear.speed -= closing * 0.7f;
				front.speed += closing * 0.2f;
			}
		}

		// And to the player, who feels it: shoved aside, braked when
		// running into one, pushed on when run into. Not in the air above.
		const float dx = a.x - (float)player->cpos.x, dz = a.z - (float)player->cpos.z;
		if (std::fabs(dx) < WIDE && std::fabs(dz) < LONG &&
		        player->cpos.y - Course.FindYCoord(a.x, a.z) < 1.2) {
			const float side = dx > 0 || (dx == 0 && (i & 1)) ? 1.f : -1.f;
			a.x += side * std::min(WIDE - std::fabs(dx), 6.f * dt) * 0.6f;
			a.heading += side * 10.f * dt;
			player->cvel.x -= side * 10.f * dt;
			const float player_down = -(float)player->cvel.z;       // speed down the course
			if (dz < 0 && player_down > a.speed) {          // the player runs into it
				const float closing = player_down - a.speed;
				player->cvel.z += closing * 0.5f;
				a.speed += closing * 0.3f;
				if (closing > 3.f && !bumped) Sound.Play("tree_hit", 0);
			} else if (dz >= 0 && a.speed > player_down) {  // it runs into the player
				const float closing = a.speed - player_down;
				a.speed -= closing * 0.7f;
				player->cvel.z -= closing * 0.2f;
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
	// The pace of the field is the player's own: it follows them up at
	// once and down more slowly, so a fall or a brush with a tree costs
	// places and a clean run earns them back.
	player_pace += (player_speed - player_pace) * clampf(dt * (player_speed > player_pace ? 1.5f : 0.6f), 0.f, 1.f);
	if (player_pace < 7.f) player_pace = 7.f;

	++tick;
	for (int i = 0; i < racing; ++i) {
		Racer& r = racers[i];
		if (r.finished) {
			r.speed *= 0.95f;
			r.z -= r.speed * dt;
			continue;
		}
		// The band: ahead of the player it eases off, by a fifth at
		// twenty-five metres and more beyond, so that a fall is not the
		// end of the race; behind, it presses on, but by less -- a lead
		// that was driven out is kept for a while.
		const float lead = (float)player->cpos.z - r.z;        // positive: it is ahead
		const float band = lead > 25.f ? clampf(0.8f - (lead - 25.f) * 0.008f, 0.4f, 0.8f)
		                 : lead > 0 ? 1.f - lead * 0.008f
		                 : clampf(1.f - lead * 0.004f, 1.f, 1.12f);
		// Uphill and on the flat it is slower than its pace, as anyone is.
		const float here = Course.FindYCoord(r.x, r.z);
		const float ahead = Course.FindYCoord(r.x, r.z - 2.f);
		const float slope = clampf(0.9f + (here - ahead) * 0.25f, 0.7f, 1.05f);
		// Rock and the like brake it as they brake the player.
		const float target = clampf(player_pace * r.pace * band * slope * ground_pace(r.x, r.z), 3.f, 40.f);
		r.speed += clampf(target - r.speed, -8.f * dt, 5.f * dt);

		// The line: of five places across, the cheapest about a second on.
		const float look = r.z - (6.f + r.speed * 0.7f);
		if ((tick + i) % 4 == 0) {
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
		r.z -= r.speed * dt;
		// A tree it did not get round stops it as it stops the player:
		// most of its speed is gone, and it is put beside the trunk.
		for (unsigned t : ObjectsNear(true, r.z, 0.5f)) {
			const float dx = r.x - Course.CollArr[t].pt.x;
			const float trunk = Course.CollArr[t].diam * 0.5f + 0.3f;
			if (std::fabs(dx) < trunk && std::fabs(r.z - Course.CollArr[t].pt.z) < 0.5f) {
				r.x = Course.CollArr[t].pt.x + (dx < 0 ? -trunk : trunk);
				if (r.speed > 4.f) r.speed = 4.f;
			}
		}
		if (r.z <= finish) r.finished = true;
	}
	// A filmed run keeps a note of the race, twice a second: the player's
	// speed, the pace of the field and each penguin's lead in metres.
	if (PspFixedStep() && tick % 30 == 0)
		if (FILE* log = std::fopen("config/race-log.txt", tick == 30 ? "w" : "a")) {
			std::fprintf(log, "%u %.1f %.1f", tick, player_speed, player_pace);
			for (int i = 0; i < racing; ++i) std::fprintf(log, " %.1f", (float)player->cpos.z - racers[i].z);
			std::fprintf(log, "\n");
			std::fclose(log);
		}
	bumped = touching;
	touching = false;
	if (!g_game.finish) collide(dt, player, width);
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

void Draw() {
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
