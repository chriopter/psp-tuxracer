#!/usr/bin/env python3
"""Exercise the actual terrain clipping/drawing implementation on the host."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
source = (root / 'ports/extremetuxracer/src/quadtree.cpp').read_text()
implementation = source[source.index('GLubyte *VNCArray;'):source.index('void quadsquare::InitArrayCounters()')]
implementation += source[source.index('inline void quadsquare::MakeNoBlendTri('):source.index('\nvoid quadsquare::RenderAux(')]
shim = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>
using GLubyte = unsigned char;
using GLuint = unsigned;
constexpr int STRIDE_GL_ARRAY = 36;
constexpr int GL_FLOAT=0, GL_UNSIGNED_BYTE=1, GL_TRIANGLES=2, GL_UNSIGNED_INT=3;
struct Vector { float x,y,z; };
struct TPlane { Vector nml; float d; };
static TPlane planes[] = {{{1,0,0},-1},{{-1,0,0},-1},{{0,1,0},-1},
                         {{0,-1,0},-1},{{0,0,1},-1},{{0,0,-1},-1}};
const TPlane* get_view_clip_planes() { return planes; }
struct quadsquare {
 static GLuint *VertexArrayIndices;
 static GLuint VertexArrayCounter;
 static void DrawTris();
 static void MakeNoBlendTri(int,int,int,int);
};
GLuint VertexIndices[9]={0,1,2};
int VertexTerrains[9];
static std::vector<std::vector<GLuint>> opaqueTerrainIndices(8);
#define colorval(j,ch) VNCArray[(j)*STRIDE_GL_ARRAY+8+(ch)]
#define setalphaval(i) colorval(VertexIndices[i],3)=(terrain<=VertexTerrains[i])?255:0
#define update_min_max(i) do {} while(0)
GLuint* quadsquare::VertexArrayIndices;
GLuint quadsquare::VertexArrayCounter;
void glTexCoordPointer(int,int,int,const void*) {}
void glFlush() {}
void PspProfileTerrain(unsigned) {}
void glColorPointer(int,int,int,const void*) {}
void glNormalPointer(int,int,const void*) {}
static const unsigned char* active_vertices;
void glVertexPointer(int,int,int,const void* p) {
 active_vertices=static_cast<const unsigned char*>(p)-24;
}
void glDrawElements(int, unsigned, int, const void*);
void glDrawArrays(int, int, unsigned);
'''
test = r'''
static std::vector<TerrainVertex> drawn;
static void capture(unsigned index) {
 TerrainVertex v;
 std::memcpy(&v, active_vertices+index*36, 36);
 for (const auto& p: planes) assert(plane_distance(v,p) <= 2e-5f);
 for (float x: v.position) assert(std::isfinite(x));
 // Attributes must be interpolated, not reset at the clipped edge.
 assert(std::fabs(v.uv[0]-(v.position[0]+2)*.25f)<2e-5f);
 assert(std::fabs(v.uv[1]-(v.position[1]+2)*.25f)<2e-5f);
 assert(v.normal[0]==0 && v.normal[1]==0 && v.normal[2]==1);
 assert(v.color[0]==120 && v.color[1]==80 && v.color[2]==40);
 assert(std::fabs(v.color[3]-(126+v.position[0]*8)) < 4);
 drawn.push_back(v);
}
void glDrawElements(int, unsigned count, int, const void* p) {
 const unsigned* indices=static_cast<const unsigned*>(p);
 for(unsigned i=0;i<count;++i) capture(indices[i]);
}
void glDrawArrays(int, int first, unsigned count) {
 for(unsigned i=0;i<count;++i) capture(first+i);
}
static void run(float ax,float ay,float bx,float by,float cx,float cy,float az=0,float bz=0,float cz=0) {
 TerrainVertex vertices[3] = {};
 float xy[]={ax,ay,bx,by,cx,cy};
 float z[]={az,bz,cz};
 for(int i=0;i<3;++i) {
  auto& v=vertices[i];
  v.position[0]=xy[2*i];v.position[1]=xy[2*i+1];v.position[2]=z[i];
  v.uv[0]=(v.position[0]+2)*.25f;v.uv[1]=(v.position[1]+2)*.25f;
  v.normal[2]=1;v.color[0]=120;v.color[1]=80;v.color[2]=40;v.color[3]=(GLubyte)(126+v.position[0]*8);
 }
 unsigned indices[]={0,1,2};
 VNCArray=reinterpret_cast<GLubyte*>(vertices);
 terrain_pointers(vertices);
 quadsquare::VertexArrayIndices=indices;quadsquare::VertexArrayCounter=3;
 drawn.clear();quadsquare::DrawTris();
 assert(active_vertices==reinterpret_cast<const unsigned char*>(vertices));
 assert(drawn.size()%3==0);
}
static float area() {
 float result=0;
 for(unsigned i=0;i<drawn.size();i+=3) {
  const float* a=drawn[i].position;const float* b=drawn[i+1].position;const float* c=drawn[i+2].position;
  float cross=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
  assert(cross >= -1e-5f); // preserve front-face winding
  result+=cross*.5f;
 }
 return result;
}
int main() {
 // The one-pass opaque buckets must exactly match the original repeated
 // per-material selection for every combination of three terrain corners.
 for(int a=0;a<8;++a)for(int b=0;b<8;++b)for(int c=0;c<8;++c) {
  GLubyte vertices[3*STRIDE_GL_ARRAY]={};GLuint indices[3]={};
  VNCArray=vertices;quadsquare::VertexArrayIndices=indices;
  VertexTerrains[0]=a;VertexTerrains[1]=b;VertexTerrains[2]=c;
  for(auto& bucket:opaqueTerrainIndices)bucket.clear();
  quadsquare::MakeNoBlendTri(0,1,2,-2);
  for(int material=0;material<8;++material) {
   quadsquare::VertexArrayCounter=0;
   quadsquare::MakeNoBlendTri(0,1,2,material);
   assert(opaqueTerrainIndices[material].size()==quadsquare::VertexArrayCounter);
   for(unsigned i=0;i<quadsquare::VertexArrayCounter;++i)
    assert(opaqueTerrainIndices[material][i]==indices[i]);
  }
 }
 run(0,0,.5f,0,0,.5f);assert(drawn.size()==3 && std::fabs(area()-.125f)<1e-6f);
 run(-2,-2,4,-2,-2,4);assert(std::fabs(area()-4)<1e-5f);
 run(2,2,3,2,2,3);assert(drawn.empty());
 run(0,0,.5f,0,0,.5f,2,2,2);assert(drawn.empty());
 run(0,0,1,0,0,1,0,2,0);assert(std::fabs(area()-.375f)<1e-5f);
 run(-1,-1,1,-1,-1,1);assert(std::fabs(area()-2)<1e-5f);
 // The same clipper handles a whole skybox face, not just triangles.
 TerrainVertex quad[12] = {};
 float corners[][3]={{-2,-2,0},{2,-2,0},{2,2,0},{-2,2,0}};
 for(int i=0;i<4;++i) std::copy(corners[i],corners[i]+3,quad[i].position);
 int count=clip_polygon(quad,4,planes,63,terrain_lerp);
 assert(count==4);
 for(int i=0;i<count;++i) {
  assert(std::fabs(std::fabs(quad[i].position[0])-1)<1e-5f);
  assert(std::fabs(std::fabs(quad[i].position[1])-1)<1e-5f);
  for(const auto& p:planes) assert(plane_distance(quad[i],p)<=1e-5f);
 }
 std::mt19937 rng(42);std::uniform_real_distribution<float> d(-8,8);
 for(int i=0;i<10000;++i) run(d(rng),d(rng),d(rng),d(rng),d(rng),d(rng),d(rng),d(rng),d(rng));
 puts("PASS: terrain clipping, rejection, winding, area, UV/color/normal interpolation and 10000 boundary cases");
}
'''
with tempfile.TemporaryDirectory(prefix='etr-terrain-') as tmp:
    directory = Path(tmp)
    cpp = directory / 'test.cpp'
    (directory / 'clip_polygon.h').write_bytes((root / 'ports/extremetuxracer/src/clip_polygon.h').read_bytes())
    (directory / 'view.h').write_text('// TPlane supplied by the test harness.\n')
    cpp.write_text(shim + '\n#include "clip_polygon.h"\n' + implementation + test)
    executable = directory / 'test'
    subprocess.run(['g++', '-std=c++17', '-O2', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', str(cpp), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
