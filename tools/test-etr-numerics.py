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
scale_method=(src/'tux.cpp').read_text().split('void CCharShape::ScaleNode(std::size_t node_name, const TVector3d& vec) {',1)[1].split('\nbool CCharShape::VisibleNode',1)[0]
scale_fixture=r'''
#include "mathlib.h"
struct TCharNode { TMatrix<4,4> trans, invtrans; };
struct CCharShape {
 TCharNode node; bool newActions=false, useActions=false;
 TCharNode* GetNode(std::size_t) { return &node; }
 void AddAction(std::size_t,int,const TVector3d&,int) {}
 void ScaleNode(std::size_t node_name, const TVector3d& vec);
};
void CCharShape::ScaleNode(std::size_t node_name, const TVector3d& vec) {
'''+scale_method
view_source=(src/'view.cpp').read_text()
view_factor=view_source.split('float time_constant_mult = ',1)[1].split(';',1)[0]
view_defines='\n'.join(line for line in view_source.splitlines() if line.startswith(('#define BASELINE_INTERPOLATION_SPEED ', '#define NO_INTERPOLATION_SPEED ')))
test=scale_fixture+'\n'+view_defines+'\nfloat camera_factor(float speed) { return '+view_factor+'; }\n'+r'''
#include "mathlib.h"
#include <cstdio>
int main(){
 for(float speed : {0.f,1.f,2.f,2.01f,4.5f,100.f}) {
  assert(std::isfinite(camera_factor(speed)) && camera_factor(speed)>=1.f);
 }
 for(float z : {0.f,-0.f,1.e-9f,-1.e-9f,.25f,-.25f}) {
  CCharShape shape;shape.node.trans.SetIdentity();shape.node.invtrans.SetIdentity();
  shape.ScaleNode(81,TVector3d(.36f,.01f,z));
  auto product=shape.node.trans*shape.node.invtrans;
  for(int r=0;r<4;r++)for(int c=0;c<4;c++) {
   assert(std::isfinite(shape.node.trans[r][c]));
   assert(std::isfinite(shape.node.invtrans[r][c]));
   assert(std::fabs(product[r][c]-(r==c?1.f:0.f))<1.e-5f);
  }
  assert(std::signbit(shape.node.trans[2][2])==std::signbit(z));
 }
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
