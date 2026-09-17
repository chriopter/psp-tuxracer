#!/usr/bin/env python3
"""Test actual ETR PSP numerical sources against analytic solutions."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parent.parent
src=root/'ports/extremetuxracer/src'
shim=r'''
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <cassert>
#include "vectors.h"
#define EPS 1.0e-13f
#define MAG_SQD(vec) ((vec).x*(vec).x+(vec).y*(vec).y+(vec).z*(vec).z)
#define ANGLES_TO_RADIANS(x) (3.141592653589793f/180.f*(x))
#define RADIANS_TO_ANGLES(x) (180.f/3.141592653589793f*(x))
#define clamp(minimum,x,maximum) (std::max(std::min(x,maximum),minimum))
'''
test=r'''
#include "mathlib.h"
#include <cstdio>
int main(){
 TOdeSolver solver;TOdeData data;float value=1;
 for(int frame=0;frame<500;frame++){
  solver.InitOdeData(&data,value,.01f);
  for(int stage=0;stage<solver.NumEstimates();stage++)
   solver.UpdateEstimate(&data,stage,-2*solver.NextValue(&data,stage));
  value=solver.FinalEstimate(&data);assert(std::isfinite(value)&&value>0&&value<=1);
 }
 assert(std::fabs(value-std::exp(-10.f))<1e-7f);
 float a[]={2,1,-1,8,-3,-1,2,-11,-2,1,2,-3},x[3];
 assert(Gauss(a,3,x)==0);assert(std::fabs(x[0]-2)<1e-5f&&std::fabs(x[1]-3)<1e-5f&&std::fabs(x[2]+1)<1e-5f);
 float singular[]={1,0,1,2,0,1,1,2,1,1,2,4};assert(Gauss(singular,3,x)!=0);
 TVector3d p;assert(IntersectPlanes(TPlane(1,0,0,-2),TPlane(0,1,0,-3),TPlane(0,0,1,4),&p));
 assert(std::fabs(p.x-2)<1e-6f&&std::fabs(p.y-3)<1e-6f&&std::fabs(p.z+4)<1e-6f);
 assert(!IntersectPlanes(TPlane(1,0,0,-2),TPlane(1,0,0,-3),TPlane(0,0,1,4),&p));
 TMatrix<4,4> translation,rotation;translation.SetTranslationMatrix(10,-4,2);rotation.SetRotationMatrix(90,'z');
 p=TransformPoint(translation*rotation,TVector3d(1,0,0));
 assert(std::fabs(p.x-10)<1e-5f&&std::fabs(p.y+3)<1e-5f&&std::fabs(p.z-2)<1e-5f);
 auto q=MakeRotationQuaternion(TVector3d(1,0,0),TVector3d(0,1,0));p=RotateVector(q,TVector3d(1,0,0));
 assert(std::fabs(p.x)<1e-5f&&std::fabs(p.y-1)<1e-5f&&std::fabs(p.z)<1e-5f);
 puts("PASS: ETR ODE integration, pivoted/singular systems, plane intersections, transforms and quaternions");
}
'''
with tempfile.TemporaryDirectory(prefix='etr-numerics-') as tmp:
 d=Path(tmp)
 for name in ('mathlib.cpp','mathlib.h','matrices.cpp','matrices.h','vectors.cpp','vectors.h'):(d/name).write_bytes((src/name).read_bytes())
 (d/'bh.h').write_text(shim);(d/'common.h').write_text(shim);(d/'test.cpp').write_text(test)
 exe=d/'test'
 subprocess.run(['g++','-std=c++17','-O2','-fsingle-precision-constant','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(d),str(d/'test.cpp'),str(d/'mathlib.cpp'),str(d/'matrices.cpp'),str(d/'vectors.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
