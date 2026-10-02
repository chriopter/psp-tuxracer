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


#ifndef VIEW_H
#define VIEW_H

#include "bh.h"
#include "mathlib.h"

void set_view_mode(CControl *ctrl, TViewMode mode);
void update_view(CControl *ctrl, float dt);

void SetStationaryCamera(bool stat);  // 0 follow, 1 stationary
void IncCameraDistance(float timestep);
void SetCameraDistance(float val);

// ------------- viewfrustum ------------------------------------------

enum clip_result_t {
	NoClip,
	SomeClip,
	NotVisible
};

void SetupViewFrustum(const CControl *ctrl);
clip_result_t clip_aabb_to_view_frustum(const TVector3d& min, const TVector3d& max);

const TPlane& get_far_clip_plane();
const TPlane* get_view_clip_planes();
// The same near and far planes, with the four side planes opened to
// GUARD_BAND times the view's tangent. The GE cuts at the screen edge by
// itself as long as a projected vertex stays inside its coordinate range,
// so geometry needs clipping on the CPU only where it leaves this wider
// frustum -- or crosses the near or far plane.
const TPlane* get_guard_clip_planes();
const TPlane& get_left_clip_plane();
const TPlane& get_right_clip_plane();
const TPlane& get_bottom_clip_plane();

#endif
