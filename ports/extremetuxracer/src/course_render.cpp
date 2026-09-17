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

void RenderCourse() {
	ScopedRenderMode rm(COURSE);
	if (param.perf_level == 1) glDisable(GL_BLEND);
	setup_course_tex_gen();
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	set_material(colWhite, colBlack, 1.0);
	const CControl *ctrl = g_game.player->ctrl;
	UpdateQuadtree(ctrl->viewpos, param.course_detail_level);
	RenderQuadtree();
}

void DrawTrees() {
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
	// Trees
	for (std::size_t i = 0; i< Course.CollArr.size(); i++) {
		if (clip_course) {
			if (ctrl->viewpos.z - Course.CollArr[i].pt.z > fwd_clip_limit) continue;
			if (Course.CollArr[i].pt.z - ctrl->viewpos.z > bwd_clip_limit) continue;
			const auto& tree=Course.CollArr[i];
			const float radius=tree.diam*0.75f; // Includes the optional 1-degree rotation.
			if (clip_aabb_to_view_frustum(tree.pt+TVector3d(-radius,0,-radius),
			                            tree.pt+TVector3d(radius,tree.height,radius))==NotVisible) continue;
		}

		if (Course.CollArr[i].tree_type != tree_type) {
			flush();
			tree_type = Course.CollArr[i].tree_type;
			Course.ObjTypes[tree_type].texture->Bind();
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

	// Items
	const TObjectType* item_type = nullptr;

	for (std::size_t i = 0; i< Course.NocollArr.size(); i++) {
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
}
