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

#include <algorithm>
#include <vector>
#include <utility>
#include "psp_profile.h"
#include "psp_buffers.h"
#include "textures.h"
#include "course_render.h"
#include "course.h"
#include "ogl.h"
#include "quadtree.h"
#include "particles.h"
#include "env.h"
#include "game_ctrl.h"
#include "physics.h"

#define TEX_SCALE 6
static const bool clip_course = true;

void setup_course_tex_gen() {
	static const GLfloat xplane[4] = {1.f / TEX_SCALE, 0.f, 0.f, 0.f };
	static const GLfloat zplane[4] = {0.f, 0.f, 1.f / TEX_SCALE, 0.f };
	glTexGenfv(GL_S, GL_OBJECT_PLANE, xplane);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, zplane);
}

void RenderCourse(bool quadtree_updated) {
	ScopedRenderMode rm(COURSE);
	if (param.perf_level == 1) glDisable(GL_BLEND);
	setup_course_tex_gen();
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	set_material(colWhite, colBlack, 1.0);
	const CControl *ctrl = g_game.player->ctrl;
	if (!quadtree_updated) UpdateQuadtree(ctrl->viewpos, param.course_detail_level);
	RenderQuadtree();
}

// The objects of the course in the order of their z, built once a course: a
// frame then looks only at those between the clip distances instead of at
// every tree and item there is. What it finds is put back in the order of
// the arrays, so the draw order and texture runs stay as they were.
static std::vector<std::pair<float, unsigned>> trees_by_z, items_by_z;
static bool objects_indexed = false;
// Every tree's two crossed quads, built once a course and kept in a buffer
// object: a tree in view is then twelve indices, not twelve vertices put
// together again each frame. Left out where a course has more trees than
// 16-bit indices reach or the buffer cannot be had; the old path draws then.
struct TreeVertex { float uv[2], normal[3], position[3]; };
static GLuint treeBuffer = 0;
static bool treeBufferUsable = false;
void InvalidateObjectIndex() { objects_indexed = false; }

template<class Array>
static void index_by_z(const Array& objects, std::vector<std::pair<float, unsigned>>& out) {
	out.clear();
	out.reserve(objects.size());
	for (unsigned i = 0; i < objects.size(); ++i) out.push_back({(float)objects[i].pt.z, i});
	std::sort(out.begin(), out.end());
}

static void in_z_range(const std::vector<std::pair<float, unsigned>>& sorted, float from, float to,
                       std::vector<unsigned>& out) {
	out.clear();
	auto it = std::lower_bound(sorted.begin(), sorted.end(), std::make_pair(from, 0u));
	for (; it != sorted.end() && it->first <= to; ++it) out.push_back(it->second);
	std::sort(out.begin(), out.end());
}

static bool trees_stale = false;
static float widest_tree = 0, widest_item = 0;
static void index_objects() {
	if (objects_indexed) return;
	index_by_z(Course.CollArr, trees_by_z);
	index_by_z(Course.NocollArr, items_by_z);
	widest_tree = widest_item = 0;
	for (const auto& tree : Course.CollArr) widest_tree = std::max(widest_tree, (float)tree.diam);
	for (const auto& item : Course.NocollArr) widest_item = std::max(widest_item, (float)item.diam);
	objects_indexed = true;
	trees_stale = true;
}

// The trees or items whose z lies within reach of a point, in the order of
// their array: what the collision checks ask instead of walking them all.
// reach is added to half the widest object of the kind.
const std::vector<unsigned>& ObjectsNear(bool trees, float z, float reach) {
	static std::vector<unsigned> near_tree, near_item;
	index_objects();
	const float r = reach + (trees ? widest_tree : widest_item) * 0.5f;
	std::vector<unsigned>& out = trees ? near_tree : near_item;
	in_z_range(trees ? trees_by_z : items_by_z, z - r, z + r, out);
	return out;
}

void DrawTrees() {
	PspProfileScope profile(PSP_SUB_TREES);
	index_objects();
	static std::vector<unsigned> near_objects;
	static std::vector<GLushort> tree_indices;
	tree_indices.clear();
	std::size_t tree_type = -1;
	const CControl*	ctrl = g_game.player->ctrl;

	ScopedRenderMode rm(TREES);
	float fwd_clip_limit = param.forward_clip_distance;
	float bwd_clip_limit = param.backward_clip_distance;

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	set_material(colWhite, colBlack, 1.0);

	struct ObjectVertex { float uv[2], normal[3], position[3]; };
	static std::vector<ObjectVertex> batch;
	batch.clear();
	auto flush = [&]() {
		if (batch.empty()) return;
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, sizeof(ObjectVertex), batch[0].uv);
		glNormalPointer(GL_FLOAT, sizeof(ObjectVertex), batch[0].normal);
		glVertexPointer(3, GL_FLOAT, sizeof(ObjectVertex), batch[0].position);
		glDrawArrays(GL_TRIANGLES, 0, batch.size());
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
		batch.clear();
	};
	auto append = [&](const GLfloat* vtx, const GLshort* tex, const TVector3d& pos,
	                  const TVector3d& normal, float sine=0.f, float cosine=1.f) {
		for (int j : {0,1,2,0,2,3}) {
			const float x=vtx[j*3], z=vtx[j*3+2];
			batch.push_back({{float(tex[j*2]),float(tex[j*2+1])},
			                {normal.x,normal.y,normal.z},
			                {pos.x+x*cosine+z*sine,pos.y+vtx[j*3+1],pos.z-x*sine+z*cosine}});
		}
	};
	const float sine = param.perf_level>1 ? std::sin(ANGLES_TO_RADIANS(1.f)) : 0.f;
	const float cosine = param.perf_level>1 ? std::cos(ANGLES_TO_RADIANS(1.f)) : 1.f;
	if (trees_stale) {
		trees_stale = false;
		treeBufferUsable = false;
		const std::size_t count = Course.CollArr.size() * 8;
		if (count > 0 && count <= 65535) {
			if (!treeBuffer) glGenBuffers(1, &treeBuffer);
			if (treeBuffer) {
				// Written straight into the buffer: no second copy of the
				// trees on a heap that has little to spare.
				while (glGetError() != GL_NO_ERROR) {}
				glBindBuffer(GL_ARRAY_BUFFER, treeBuffer);
				glBufferData(GL_ARRAY_BUFFER, (long)(count * sizeof(TreeVertex)), nullptr, GL_DYNAMIC_DRAW);
				TreeVertex* out = glGetError() == GL_NO_ERROR
					? static_cast<TreeVertex*>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY)) : nullptr;
				if (out) {
					for (const auto& tree : Course.CollArr) {
						const float r = tree.diam / 2.0, h = tree.height;
						const float corner[8][3] = {{-r,0,0},{r,0,0},{r,h,0},{-r,h,0},{0,0,-r},{0,0,r},{0,h,r},{0,h,-r}};
						static const float uv[4][2] = {{0,1},{1,1},{1,0},{0,0}};
						for (int j = 0; j < 8; ++j) {
							const float x = corner[j][0], z = corner[j][2];
							*out++ = {{uv[j&3][0], uv[j&3][1]}, {sine, 0, cosine},
							          {float(tree.pt.x+x*cosine+z*sine), float(tree.pt.y+corner[j][1]),
							           float(tree.pt.z-x*sine+z*cosine)}};
						}
					}
					glUnmapBuffer(GL_ARRAY_BUFFER);
					treeBufferUsable = true;
				}
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}
		}
	}
	auto flush_trees = [&]() {
		if (tree_indices.empty()) return;
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, treeBuffer);
		glTexCoordPointer(2, GL_FLOAT, sizeof(TreeVertex), (const GLvoid*)0);
		glNormalPointer(GL_FLOAT, sizeof(TreeVertex), (const GLvoid*)8);
		glVertexPointer(3, GL_FLOAT, sizeof(TreeVertex), (const GLvoid*)20);
		glDrawElements(GL_TRIANGLES, tree_indices.size(), GL_UNSIGNED_SHORT, tree_indices.data());
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		// Pointers back into memory, so PSPGL lets go of the buffer.
		static const TreeVertex none = {};
		glTexCoordPointer(2, GL_FLOAT, sizeof(TreeVertex), none.uv);
		glNormalPointer(GL_FLOAT, sizeof(TreeVertex), none.normal);
		glVertexPointer(3, GL_FLOAT, sizeof(TreeVertex), none.position);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
		tree_indices.clear();
	};
	// Trees
	in_z_range(trees_by_z, ctrl->viewpos.z - fwd_clip_limit, ctrl->viewpos.z + bwd_clip_limit, near_objects);
	for (unsigned i : near_objects) {
		if (clip_course) {
			if (ctrl->viewpos.z - Course.CollArr[i].pt.z > fwd_clip_limit) continue;
			if (Course.CollArr[i].pt.z - ctrl->viewpos.z > bwd_clip_limit) continue;
			const auto& tree=Course.CollArr[i];
			const float radius=tree.diam*0.75f; // Includes the optional 1-degree rotation.
			if (clip_aabb_to_view_frustum(tree.pt+TVector3d(-radius,0,-radius),
			                            tree.pt+TVector3d(radius,tree.height,radius))==NotVisible) continue;
		}

		if (PspProfileActive()) PspProfileAdd(PSP_N_TREES_DRAWN, 1);
		if (Course.CollArr[i].tree_type != tree_type) {
			flush();
			flush_trees();
			tree_type = Course.CollArr[i].tree_type;
			Course.ObjTypes[tree_type].texture->Bind();
		}

		if (treeBufferUsable) {
			const unsigned base = i * 8;
			for (int j : {0,1,2,0,2,3,4,5,6,4,6,7}) tree_indices.push_back((GLushort)(base + j));
			continue;
		}
		float treeRadius = Course.CollArr[i].diam / 2.0;
		float treeHeight = Course.CollArr[i].height;

		static const GLshort tex[] = {
			0, 1,
			1, 1,
			1, 0,
			0, 0,
			0, 1,
			1, 1,
			1, 0,
			0, 0
		};

		const GLfloat vtx[] = {
			-treeRadius, 0.0,        0.0,
			    treeRadius,  0.0,        0.0,
			    treeRadius,  treeHeight, 0.0,
			    -treeRadius, treeHeight, 0.0,
			    0.0,         0.0,        -treeRadius,
			    0.0,         0.0,        treeRadius,
			    0.0,         treeHeight, treeRadius,
			    0.0,         treeHeight, -treeRadius
		    };

		const TVector3d normal(sine,0,cosine);
		append(vtx,tex,Course.CollArr[i].pt,normal,sine,cosine);
		append(vtx+12,tex+8,Course.CollArr[i].pt,normal,sine,cosine);
	}
	flush();
	flush_trees();

	// Items
	const TObjectType* item_type = nullptr;

	unsigned long long items_start = PspProfileActive() ? PspProfileNow() : 0;
	in_z_range(items_by_z, ctrl->viewpos.z - fwd_clip_limit, ctrl->viewpos.z + bwd_clip_limit, near_objects);
	for (unsigned i : near_objects) {
		if (Course.NocollArr[i].collectable == 0 || Course.NocollArr[i].type.drawable == false) continue;
		if (clip_course) {
			if (ctrl->viewpos.z - Course.NocollArr[i].pt.z > fwd_clip_limit) continue;
			if (Course.NocollArr[i].pt.z - ctrl->viewpos.z > bwd_clip_limit) continue;
			const auto& item=Course.NocollArr[i];
			const float radius=item.diam*0.5f;
			if (clip_aabb_to_view_frustum(item.pt+TVector3d(-radius,0,-radius),
			                            item.pt+TVector3d(radius,item.height,radius))==NotVisible) continue;
		}

		if (&Course.NocollArr[i].type != item_type) {
			flush();
			item_type = &Course.NocollArr[i].type;
			item_type->texture->Bind();
		}

		float itemRadius = Course.NocollArr[i].diam / 2;
		float itemHeight = Course.NocollArr[i].height;

		TVector3d normal;
		if (item_type->use_normal) {
			normal = item_type->normal;
		} else {
			normal = ctrl->viewpos - Course.NocollArr[i].pt;
			normal.Norm();
		}
		const TVector3d lightNormal = normal;
		normal.y = 0.0;
		normal.Norm();

		static const GLshort tex[] = {
			0, 1,
			1, 1,
			1, 0,
			0, 0
		};

		const GLfloat vtx[] = {
			static_cast<GLfloat>(-itemRadius*normal.z),
			0.f,
			static_cast<GLfloat>(itemRadius*normal.x),

			static_cast<GLfloat>(itemRadius*normal.z),
			0.f,
			static_cast<GLfloat>(-itemRadius*normal.x),
			static_cast<GLfloat>(itemRadius*normal.z),
			static_cast<GLfloat>(itemHeight),
			static_cast<GLfloat>(-itemRadius*normal.x),
			static_cast<GLfloat>(-itemRadius*normal.z),
			static_cast<GLfloat>(itemHeight),
			static_cast<GLfloat>(itemRadius*normal.x)
		};
		append(vtx,tex,Course.NocollArr[i].pt,lightNormal);
	}
	flush();
	if (items_start) PspProfileAdd(PSP_SUB_ITEMS, PspProfileNow() - items_start);
}
