/* --------------------------------------------------------------------
EXTREME TUXRACER

Copyright (C) 1999-2001 Jasmin F. Patry (Tuxracer)
Copyright (C) 2004-2005 Volker Stroebel (Planetpenguin Racer)
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


#ifndef MATHLIB_H
#define MATHLIB_H

#include "bh.h"
#include "matrices.h"

static constexpr TVector3d GravVec(0.0, -1.0, 0.0);

// --------------------------------------------------------------------
//			Advanced geometry
// --------------------------------------------------------------------

struct TPlane {
	TVector3d nml;
	float d;
	constexpr explicit TPlane(float nx = 0.0, float ny = 0.0, float nz = 0.0, float d_ = 0.0)
		: nml(nx, ny, nz), d(d_)
	{}
};

struct TPolygon		{ std::vector<int> vertices; };
struct TRay			{ TVector3d pt; TVector3d vec; };

struct TPolyhedron {
	std::vector<TVector3d> vertices;
	std::vector<TPolygon> polygons;
};

TVector3d	ProjectToPlane(const TVector3d& nml, const TVector3d& v);
TVector3d	TransformVector(const TMatrix<4, 4>& mat, const TVector3d& v);
TVector3d	TransformNormal(const TVector3d& n, const TMatrix<4, 4>& mat);	// not used ?
TVector3d	TransformPoint(const TMatrix<4, 4>& mat, const TVector3d& p);
bool		IntersectPlanes(const TPlane& s1, const TPlane& s2, const TPlane& s3, TVector3d *p);
float		DistanceToPlane(const TPlane& plane, const TVector3d& pt);

TMatrix<4, 4> RotateAboutVectorMatrix(const TVector3d& u, float angle);

TQuaternion MultiplyQuaternions(const TQuaternion& q, const TQuaternion& r);
TQuaternion ConjugateQuaternion(const TQuaternion& q);
TMatrix<4, 4> MakeMatrixFromQuaternion(const TQuaternion& q);
TQuaternion MakeQuaternionFromMatrix(const TMatrix<4, 4>& m);
TQuaternion MakeRotationQuaternion(const TVector3d& s, const TVector3d& t);
TQuaternion InterpolateQuaternions(const TQuaternion& q, TQuaternion r, float t);
TVector3d	RotateVector(const TQuaternion& q, const TVector3d& v);

bool		IntersectPolygon(const TPolygon& p, std::vector<TVector3d>& v);
bool		IntersectPolyhedron(TPolyhedron& p);
TVector3d	MakeNormal(const TPolygon& p, const TVector3d *v);
void		TransPolyhedron(const TMatrix<4, 4>& mat, TPolyhedron& ph);

// --------------------------------------------------------------------
//				ode solver
// --------------------------------------------------------------------

struct TOdeData {
	float k[4];
	float init_val;
	float h;
};

typedef int (*PNumEstimates)();
typedef void (*PInitOdeData)(TOdeData *, float init_val, float h);
typedef float (*PNextTime)(TOdeData *, int step);
typedef float (*PNextValue)(TOdeData *, int step);
typedef void (*PUpdateEstimate)(TOdeData *, int step, float val);
typedef float (*PFinalEstimate)(TOdeData *);
typedef float (*PEstimateError)(TOdeData *);
typedef float (*PTimestepExponent)();

struct TOdeSolver {
	PNumEstimates		NumEstimates;
	PInitOdeData		InitOdeData;
	PNextTime			NextTime;
	PNextValue			NextValue;
	PUpdateEstimate		UpdateEstimate;
	PFinalEstimate		FinalEstimate;
	PEstimateError		EstimateError;
	PTimestepExponent	TimestepExponent;
	TOdeSolver();
};

// --------------------------------------------------------------------
//			special
// --------------------------------------------------------------------

int Gauss(float *matrix, int n, float *soln);
float LinearInterp(const float x[], const float y[], float val, int n);

float	XRandom(float min, float max);
float	FRandom();
int		IRandom(int min, int max);
int		ITrunc(int val, int base);
int		IFrac(int val, int base);

#endif
