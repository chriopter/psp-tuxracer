// PSP clipping before perspective divide. A clipped convex triangle/quad has
// at most 9/10 vertices after six planes; callers provide room for 12.
#ifndef ETR_CLIP_POLYGON_H
#define ETR_CLIP_POLYGON_H

#include "view.h"
#include <algorithm>

template<class Vertex>
float clip_distance(const Vertex& v, const TPlane& p) {
	return v.position[0] * p.nml.x + v.position[1] * p.nml.y +
	       v.position[2] * p.nml.z + p.d;
}

template<class Vertex, class Interpolate>
int clip_polygon(Vertex* polygon, int count, const TPlane* planes,
                 unsigned boundary, Interpolate interpolate) {
	Vertex output[12];
	for (int p = 0; p < 6 && count; ++p) {
		if (!(boundary & (1u << p))) continue;
		int n = 0;
		Vertex previous = polygon[count-1];
		float dp = clip_distance(previous, planes[p]);
		for (int j = 0; j < count; ++j) {
			const Vertex& current = polygon[j];
			float dc = clip_distance(current, planes[p]);
			if ((dp <= 0) != (dc <= 0))
				output[n++] = interpolate(previous, current, dp / (dp - dc));
			if (dc <= 0) output[n++] = current;
			previous = current;
			dp = dc;
		}
		count = n;
		std::copy(output, output+n, polygon);
	}
	return count;
}

#endif
