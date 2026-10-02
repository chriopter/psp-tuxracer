/* --------------------------------------------------------------------
EXTREME TUXRACER

Copyright (C) 1999-2001 Jasmin F. Patry (Tuxracer)
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

#include "psp_profile.h"
#include "track_marks.h"
#include "game_ctrl.h"
#include "winsys.h"
#include "ogl.h"
#include "textures.h"
#include "course.h"
#include "physics.h"
#include "clip_polygon.h"
#include <list>
#include <vector>
#include <iterator>

#define TRACK_WIDTH 0.7
// A ring of the newest marks. 4096 is over a minute of trench at 60 frames a
// second, far more than the view distance ever shows behind the player, and
// bounds what a long race takes from the PSP-1000's heap (10000 before).
#define MAX_TRACK_MARKS 4096
#define SPEED_TO_START_TRENCH 0.0
#define TRACK_HEIGHT 0.08
#define MAX_TRACK_DEPTH 0.7


enum track_types_t {
	TRACK_HEAD,
	TRACK_MARK,
	TRACK_TAIL,
	NUM_TRACK_TYPES
};

struct track_quad_t {
	TVector3d v1, v2, v3, v4;
	TVector2d t1, t2, t3, t4;
	TVector3d n1, n2, n3, n4;
	track_types_t track_type;
	uint8_t alpha;
};

struct track_marks_t {
	std::list<track_quad_t> quads;
	std::list<track_quad_t>::iterator current_mark;
};

static track_marks_t track_marks;
static bool continuing_track;

// The marks in the order they were laid, in runs of TRACK_CHUNK with the box
// each run lies in. Drawing asks the box first, so a long race does not walk
// every mark it has ever laid to find the few in view. A box only grows: when
// the ring starts over, a run's box also covers the marks it held before.
#define TRACK_CHUNK 64
// Longer than any mark: a frame's travel at top speed is under two metres.
#define TRACK_REACH 4.f
struct track_chunk_t {
	std::list<track_quad_t>::iterator first;
	int count;
	float min[3], max[3];
};
static std::vector<track_chunk_t> track_chunks;
static std::size_t track_index;
// A mark's first corner and where the mark is, in the order laid and side by
// side in memory: the walk that finds the marks in view reads these and
// leaves the marks themselves, a list node each, alone until one is drawn.
struct track_key_t { float x, y, z; const track_quad_t* quad; };
static std::vector<track_key_t> track_keys;

static void chunk_cover(track_chunk_t& c, const TVector3d& v) {
	const float p[3] = {(float)v.x, (float)v.y, (float)v.z};
	for (int k = 0; k < 3; ++k) {
		if (p[k] < c.min[k]) c.min[k] = p[k];
		if (p[k] > c.max[k]) c.max[k] = p[k];
	}
}

static int trackid1 = 1;
static int trackid2 = 2;
static int trackid3 = 3;

void SetTrackIDs(int id1, int id2, int id3) {
	trackid1 = id1;
	trackid2 = id2;
	trackid3 = id3;
}

void init_track_marks() {
	track_marks.quads.clear();
	track_chunks.clear();
	track_keys.clear();
	track_index = 0;
	track_marks.current_mark = track_marks.quads.begin();
	continuing_track = false;
}

template<typename T>
static T incrementRingIterator(T q) {
	T ret = q;
	++ret;
	if (ret == track_marks.quads.end())
		ret = track_marks.quads.begin();
	return ret;
}

template<typename T>
static T decrementRingIterator(T q) {
	T ret = q;
	if (ret == track_marks.quads.begin())
		return track_marks.quads.end();
	--ret;
	return ret;
}

void DrawTrackmarks() {
	PspProfileScope profile(PSP_SUB_TRACKMARKS);
	// The snow trench is core game feedback, including the PSP default profile.
	if (track_marks.quads.empty())
		return;

	TTexture* textures[NUM_TRACK_TYPES];

	sf::Color track_colour = colWhite;
	set_material(track_colour, colBlack, 1.0);
	ScopedRenderMode rm(TRACK_MARKS);

	textures[TRACK_HEAD] = Tex.GetTexture(trackid1);
	textures[TRACK_MARK] = Tex.GetTexture(trackid2);
	textures[TRACK_TAIL] = Tex.GetTexture(trackid3);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	struct TrackVertex {
		float uv[2]; GLubyte color[4]; float normal[3], position[3];
	};
	static_assert(sizeof(TrackVertex)==36, "native PSP track vertex");
	static std::vector<TrackVertex> batch;
	batch.clear();
	int bound_type=-1;
	auto flush = [&]() {
		if (batch.empty()) return;
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);
		glTexCoordPointer(2,GL_FLOAT,sizeof(TrackVertex),batch[0].uv);
		glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(TrackVertex),batch[0].color);
		glNormalPointer(GL_FLOAT,sizeof(TrackVertex),batch[0].normal);
		glVertexPointer(3,GL_FLOAT,sizeof(TrackVertex),batch[0].position);
		glDrawArrays(GL_TRIANGLES,0,batch.size());
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
		batch.clear();
	};
	const TPlane* planes=get_view_clip_planes();
	const TPlane* guards=get_guard_clip_planes();
	auto interpolate=[](const TrackVertex& a,const TrackVertex& b,float t) {
		TrackVertex v;
		for(int k=0;k<2;++k)v.uv[k]=a.uv[k]+t*(b.uv[k]-a.uv[k]);
		for(int k=0;k<4;++k)v.color[k]=a.color[k]+t*(b.color[k]-a.color[k]);
		for(int k=0;k<3;++k){
			v.normal[k]=a.normal[k]+t*(b.normal[k]-a.normal[k]);
			v.position[k]=a.position[k]+t*(b.position[k]-a.position[k]);
		}
		return v;
	};
	// The view frustum lies inside a sphere around the eye: the far distance
	// over the cosine of the half diagonal field of view. A quad wholly
	// outside that sphere fails the plane tests below as well, so skipping
	// it here changes nothing drawn and spares a long race the six plane
	// tests for every mark it has ever laid.
	const TVector3d eye = g_game.player->ctrl->viewpos;
	const float half_v = std::tan(ANGLES_TO_RADIANS(param.fov * 0.5f));
	const float half_h = half_v * Winsys.resolution.width / Winsys.resolution.height;
	const float reach = param.forward_clip_distance * std::sqrt(1.f + half_v*half_v + half_h*half_h) + 4.f;
	const float reach2 = reach * reach;
	for (const auto& chunk : track_chunks) {
		float away2 = 0;
		const float e[3] = {(float)eye.x, (float)eye.y, (float)eye.z};
		for (int k = 0; k < 3; ++k) {
			const float d = e[k] < chunk.min[k] ? chunk.min[k] - e[k]
			              : e[k] > chunk.max[k] ? e[k] - chunk.max[k] : 0.f;
			away2 += d * d;
		}
		if (away2 > reach2) continue;
		if (clip_aabb_to_view_frustum(TVector3d(chunk.min[0], chunk.min[1], chunk.min[2]),
		                              TVector3d(chunk.max[0], chunk.max[1], chunk.max[2])) == NotVisible)
			continue;
		const track_key_t* key = &track_keys[(&chunk - &track_chunks[0]) * TRACK_CHUNK];
		for (int n = 0; n < chunk.count; ++n, ++key) {
		if (PspProfileActive()) PspProfileAdd(PSP_N_TRACK_SEEN, 1);
		// A mark is far shorter than TRACK_REACH: with its first corner
		// that far outside a plane, the other three are outside it too.
		bool far_outside = false;
		for (unsigned p = 0; p < 6 && !far_outside; ++p)
			far_outside = key->x*planes[p].nml.x + key->y*planes[p].nml.y +
			              key->z*planes[p].nml.z + planes[p].d > TRACK_REACH;
		if (far_outside) continue;
		const auto& q = *key->quad;
		if (!q.alpha || !textures[q.track_type]) continue;
		const TVector3d positions[]={q.v1,q.v2,q.v4,q.v3};
		unsigned boundary=0;
		bool rejected=false;
		for(unsigned p=0;p<6;++p) {
			unsigned outside=0;
			for(const auto& v:positions)
				outside += v.x*planes[p].nml.x+v.y*planes[p].nml.y+
				           v.z*planes[p].nml.z+planes[p].d > 0;
			if(outside==4) { rejected=true; break; }
			if(!outside) continue;
			// Near and far are cut here; a side only beyond the guard band.
			if(p<2) { boundary|=1u<<p; continue; }
			for(const auto& v:positions)
				if(v.x*guards[p].nml.x+v.y*guards[p].nml.y+v.z*guards[p].nml.z+guards[p].d > 0) {
					boundary|=1u<<p; break;
				}
		}
		if(rejected) continue; // Reject only wholly invisible quads, not old visible tracks.
		const TVector3d normals[]={q.n1,q.n2,q.n4,q.n3};
		const TVector2d uv[]={q.t1,q.t2,q.t4,q.t3};
		TrackVertex vertices[4];
		for(unsigned j=0;j<4;++j) {
			vertices[j]={{uv[j].x,uv[j].y},{255,255,255,q.alpha},
			             {normals[j].x,normals[j].y,normals[j].z},
			             {positions[j].x,positions[j].y,positions[j].z}};
		}
		if (PspProfileActive()) PspProfileAdd(PSP_N_TRACK_DRAWN, 1);
		if(bound_type!=q.track_type) {
			flush(); textures[q.track_type]->Bind(); bound_type=q.track_type;
		}
		// Retain the original diagonal, UVs, normals and depth-dependent alpha.
		for(unsigned j=1;j<3;++j) {
			TrackVertex polygon[12]={vertices[0],vertices[j],vertices[j+1]};
			int count=boundary ? clip_polygon(polygon,3,guards,boundary,interpolate) : 3;
			for(int k=1;k+1<count;++k) {
				batch.push_back(polygon[0]);batch.push_back(polygon[k]);batch.push_back(polygon[k+1]);
			}
		}
		if(batch.size()>=1536) flush();
		}
	}
	flush();
}

void break_track_marks() {
	if (!continuing_track)
		return;

	std::list<track_quad_t>::iterator q = track_marks.current_mark;
	if (q != track_marks.quads.end()) {
		q->track_type = TRACK_TAIL;
		q->t1 = TVector2d(0.0, 0.0);
		q->t2 = TVector2d(1.0, 0.0);
		q->t3 = TVector2d(0.0, 1.0);
		q->t4 = TVector2d(1.0, 1.0);
		std::list<track_quad_t>::iterator qprev = decrementRingIterator(q);
		if (qprev != track_marks.quads.end()) {
			qprev->t3.y = std::max(qprev->t3.y+0.5, qprev->t1.y+1.0);
			qprev->t4.y = std::max(qprev->t3.y+0.5, qprev->t1.y+1.0);
		}
	}
	continuing_track = false;
}

void add_track_mark(const CControl *ctrl, int *id) {
	*id = Course.GetTerrainIdx(ctrl->cpos.x, ctrl->cpos.z, 0.5);
	if (*id < 1) {
		break_track_marks();
		return;
	}

	if (!Course.TerrList[*id].trackmarks) {
		break_track_marks();
		return;
	}

	float speed = ctrl->cvel.Length();
	if (speed < SPEED_TO_START_TRENCH) {
		break_track_marks();
		return;
	}

	TVector3d width_vector = CrossProduct(ctrl->cdirection, TVector3d(0, 1, 0));
	float magnitude = width_vector.Norm();
	if (magnitude == 0) {
		break_track_marks();
		return;
	}

	TVector3d left_vector = TRACK_WIDTH/2.0 * width_vector;
	TVector3d right_vector = -TRACK_WIDTH/2.0 * width_vector;
	TVector3d left_wing =  ctrl->cpos - left_vector;
	TVector3d right_wing = ctrl->cpos - right_vector;
	float left_y = Course.FindYCoord(left_wing.x, left_wing.z);
	float right_y = Course.FindYCoord(right_wing.x, right_wing.z);

	if (std::fabs(left_y-right_y) > MAX_TRACK_DEPTH) {
		break_track_marks();
		return;
	}

	TPlane surf_plane = Course.GetLocalCoursePlane(ctrl->cpos);
	float dist_from_surface = DistanceToPlane(surf_plane, ctrl->cpos);
	float comp_depth = 0.1;
	if (dist_from_surface >= (2 * comp_depth)) {
		break_track_marks();
		return;
	}

	if (track_marks.quads.size() < MAX_TRACK_MARKS) {
		track_marks.quads.emplace_back();
		if (track_chunks.empty() || track_chunks.back().count == TRACK_CHUNK)
			track_chunks.push_back({std::prev(track_marks.quads.end()), 0,
			                        {1e30f, 1e30f, 1e30f}, {-1e30f, -1e30f, -1e30f}});
		++track_chunks.back().count;
		track_keys.push_back({0, 0, 0, &track_marks.quads.back()});
	}
	std::list<track_quad_t>::iterator qprev = track_marks.current_mark;
	if (track_marks.current_mark == track_marks.quads.end()) {
		track_marks.current_mark = track_marks.quads.begin();
		track_index = 0;
	} else {
		track_marks.current_mark = incrementRingIterator(track_marks.current_mark);
		track_index = (track_index + 1) % track_marks.quads.size();
	}
	std::list<track_quad_t>::iterator q = track_marks.current_mark;

	if (!continuing_track) {
		q->track_type = TRACK_HEAD;
		q->v1 = TVector3d(left_wing.x, left_y + TRACK_HEIGHT, left_wing.z);
		q->v2 = TVector3d(right_wing.x, right_y + TRACK_HEIGHT, right_wing.z);
		q->v3 = TVector3d(left_wing.x, left_y + TRACK_HEIGHT, left_wing.z);
		q->v4 = TVector3d(right_wing.x, right_y + TRACK_HEIGHT, right_wing.z);
		q->n1 = Course.FindCourseNormal(q->v1.x, q->v1.z);
		q->n2 = Course.FindCourseNormal(q->v2.x, q->v2.z);
		q->n3 = Course.FindCourseNormal(q->v3.x, q->v3.z);
		q->n4 = Course.FindCourseNormal(q->v4.x, q->v4.z);
		q->t1 = TVector2d(0.0, 0.0);
		q->t2 = TVector2d(1.0, 0.0);
		q->t3 = TVector2d(0.0, 1.0);
		q->t4 = TVector2d(1.0, 1.0);
	} else {
		q->track_type = TRACK_TAIL;
		q->v1 = qprev->v3;
		q->v2 = qprev->v4;
		q->v3 = TVector3d(left_wing.x, left_y + TRACK_HEIGHT, left_wing.z);
		q->v4 = TVector3d(right_wing.x, right_y + TRACK_HEIGHT, right_wing.z);
		q->n1 = qprev->n3;
		q->n2 = qprev->n4;
		q->n3 = Course.FindCourseNormal(q->v3.x, q->v3.z);
		q->n4 = Course.FindCourseNormal(q->v4.x, q->v4.z);
		q->t1 = qprev->t3;
		q->t2 = qprev->t4;
		float tex_end = speed*g_game.time_step/TRACK_WIDTH;
		q->t3 = TVector2d(0.0, q->t1.y + tex_end);
		q->t4 = TVector2d(1.0, q->t2.y + tex_end);
		if (qprev->track_type == TRACK_TAIL)
			qprev->track_type = TRACK_MARK;
	}
	q->alpha = std::min(static_cast<int>((2*comp_depth-dist_from_surface)/(4*comp_depth)*255), 255);
	track_chunk_t& chunk = track_chunks[track_index / TRACK_CHUNK];
	chunk_cover(chunk, q->v1); chunk_cover(chunk, q->v2);
	chunk_cover(chunk, q->v3); chunk_cover(chunk, q->v4);
	track_keys[track_index].x = q->v1.x;
	track_keys[track_index].y = q->v1.y;
	track_keys[track_index].z = q->v1.z;
	continuing_track = true;
}

void UpdateTrackmarks(const CControl *ctrl) {
	int trackid = -1;
	TTerrType *TerrList = &Course.TerrList[0];

	add_track_mark(ctrl, &trackid);
	if (trackid >= 0 && TerrList[trackid].trackmarks) {
		SetTrackIDs(TerrList[trackid].starttex,
		            TerrList[trackid].tracktex,
		            TerrList[trackid].stoptex);
	}
}
