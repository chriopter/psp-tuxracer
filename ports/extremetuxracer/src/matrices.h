/* --------------------------------------------------------------------
EXTREME TUXRACER

Copyright (C) 2013 Extreme Tux Racer Team

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


#ifndef MATRICES_H
#define MATRICES_H

#include "vectors.h"

template<int ix, int iy>
class TMatrix {
	float _data[ix][iy];
public:
	constexpr TMatrix() = default;
	TMatrix(const TVector3d& w1, const TVector3d& w2, const TVector3d& w3);

	float* operator[](int index) { return _data[index]; }
	constexpr const float* operator[](int index) const { return _data[index]; }
	constexpr const float* data() const { return (float*)_data; }

	void SetIdentity();
	void SetRotationMatrix(float angle, char axis);
	void SetTranslationMatrix(float x, float y, float z);
	void SetScalingMatrix(float x, float y, float z);

	TMatrix GetTransposed() const;

	static const TMatrix<ix, iy>& getIdentity();
};

template<int x, int y>
TMatrix<x, y> operator*(const TMatrix<x, y>& l, const TMatrix<x, y>& r);

#endif
