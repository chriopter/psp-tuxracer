#!/usr/bin/env python3
"""Exercise the actual terrain clipping/drawing implementation on the host."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
source = (root / 'ports/extremetuxracer/src/quadtree.cpp').read_text()
# The triangle lists of a frame, without the quadtree method between them.
implementation = source[source.index('GLuint VertexIndices[9];'):source.index('void quadsquare::InitVert(')]
implementation += source[source.index('GLubyte *VNCArray;'):source.index('void quadsquare::InitArrayCounters()')]
implementation += source[source.index('inline void quadsquare::MakeNoBlendTri('):source.index('\nvoid quadsquare::RenderAux(')]
# A PSP address fits an unsigned; a host pointer does not. Nothing else differs.
psp_cast = 'reinterpret_cast<unsigned>(memory) & ~0x40000000u'
assert implementation.count(psp_cast) == 1
implementation = implementation.replace(psp_cast, 'reinterpret_cast<std::uintptr_t>(memory) & ~std::uintptr_t(0x40000000u)')
shim = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>
#include <sys/mman.h>
#include "psp_profile.h"
using GLubyte = unsigned char;
using GLushort = unsigned short;
using GLuint = unsigned;
constexpr int STRIDE_GL_ARRAY = 36;
constexpr int GL_FLOAT=0, GL_UNSIGNED_BYTE=1, GL_TRIANGLES=2, GL_UNSIGNED_INT=3, GL_UNSIGNED_SHORT=4,
              GL_ARRAY_BUFFER=5, GL_DYNAMIC_DRAW=6, GL_WRITE_ONLY=7, GL_NO_ERROR=0;
struct Vector { float x,y,z; };
struct TPlane { Vector nml; float d; };
// A cube stands in for the view. Planes 0 and 1 are near and far: cut exactly.
// The other four are the sides: cut only beyond the wider guard planes.
static TPlane planes[] = {{{1,0,0},-1},{{-1,0,0},-1},{{0,1,0},-1},
                         {{0,-1,0},-1},{{0,0,1},-1},{{0,0,-1},-1}};
static TPlane guards[] = {{{1,0,0},-1},{{-1,0,0},-1},{{0,1,0},-3},
                         {{0,-1,0},-3},{{0,0,1},-3},{{0,0,-1},-3}};
const TPlane* get_view_clip_planes() { return planes; }
const TPlane* get_guard_clip_planes() { return guards; }
struct quadsquare {
 static GLuint *VertexArrayIndices;
 static GLuint VertexArrayCounter;
 static int RowSize, NumRows;
 static void DrawTris();
 static void MakeNoBlendTri(int,int,int,int);
};
static struct { int perf_level = 1; } param;
#define colorval(j,ch) VNCArray[(j)*STRIDE_GL_ARRAY+8+(ch)]
#define setalphaval(i) colorval(VertexIndices[i],3)=(terrain<=VertexTerrains[i])?255:0
#define update_min_max(i) do {} while(0)
GLuint* quadsquare::VertexArrayIndices;
GLuint quadsquare::VertexArrayCounter;
int quadsquare::RowSize, quadsquare::NumRows;

static bool profiling = true;
static unsigned long long profileSums[PSP_SUB_COUNT];
bool PspProfileActive() { return profiling; }
unsigned long long PspProfileNow() { return 1; }
void PspProfileAdd(unsigned slot, unsigned long long n) { assert(profiling && slot < PSP_SUB_COUNT); profileSums[slot] += n; }
static unsigned announced;
void PspProfileTerrain(unsigned count) { announced = count; }

// PSPGL's buffer objects: one buffer, in memory whose address has bit 30
// clear, handed out through its uncached alias as on the console.
static unsigned char* bufferMemory;
static const std::size_t bufferRoom = 4u << 20;
static GLuint liveBuffer, boundBuffer;
static int pendingError = GL_NO_ERROR, bufferDeletes;
static bool failGen, failData, failMap, mapped;
static long bufferBytes;
static void reserve_buffer_memory() {
 const std::uintptr_t gib = std::uintptr_t(1) << 30;
 void* span = mmap(nullptr, 3 * gib, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
 assert(span != MAP_FAILED);
 std::uintptr_t at = (reinterpret_cast<std::uintptr_t>(span) + gib - 1) & ~(gib - 1);
 if (at & gib) at += gib;
 bufferMemory = reinterpret_cast<unsigned char*>(at);
 assert(!mprotect(bufferMemory, bufferRoom, PROT_READ | PROT_WRITE));
}
void glGenBuffers(int n, GLuint* id) { assert(n == 1 && !liveBuffer); *id = liveBuffer = failGen ? 0 : 7; }
void glBindBuffer(int target, GLuint id) { assert(target == GL_ARRAY_BUFFER && (!id || id == liveBuffer)); boundBuffer = id; }
void glBufferData(int target, long bytes, const void* data, int usage) {
 assert(target == GL_ARRAY_BUFFER && boundBuffer && !data && usage == GL_DYNAMIC_DRAW);
 if (failData || bytes > long(bufferRoom)) { pendingError = 1; return; }
 bufferBytes = bytes;
}
void* glMapBuffer(int target, int access) {
 assert(target == GL_ARRAY_BUFFER && access == GL_WRITE_ONLY && boundBuffer && !mapped);
 if (failMap) return nullptr;
 mapped = true;
 return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(bufferMemory) | 0x40000000u);
}
unsigned char glUnmapBuffer(int) { assert(mapped); mapped = false; return 1; }
void glDeleteBuffers(int n, const GLuint* id) {
 assert(n == 1 && *id && *id == liveBuffer);
 if (boundBuffer == liveBuffer) boundBuffer = 0;
 liveBuffer = 0; ++bufferDeletes;
}
int glGetError() { int e = pendingError; pendingError = GL_NO_ERROR; return e; }
static const void* writtenBack; static long writtenBytes; static int writebacks;
void sceKernelDcacheWritebackRange(const void* p, unsigned n) { writtenBack = p; writtenBytes = n; ++writebacks; }

// With a buffer bound a pointer is an offset into it.
static const unsigned char* active_vertices;
static const unsigned char* pointers[4];
static const unsigned char* resolve(const void* p) {
 return boundBuffer ? bufferMemory + reinterpret_cast<std::uintptr_t>(p) : static_cast<const unsigned char*>(p);
}
void glTexCoordPointer(int size,int type,int stride,const void* p) { assert(size==2 && type==GL_FLOAT && stride==36); pointers[0]=resolve(p); }
void glColorPointer(int size,int type,int stride,const void* p) { assert(size==4 && type==GL_UNSIGNED_BYTE && stride==36); pointers[1]=resolve(p); }
void glNormalPointer(int type,int stride,const void* p) { assert(type==GL_FLOAT && stride==36); pointers[2]=resolve(p); }
void glVertexPointer(int size,int type,int stride,const void* p) {
 assert(size==3 && type==GL_FLOAT && stride==36);
 pointers[3]=resolve(p);
 active_vertices=pointers[3]-24;
}
static bool flushed;
void glFlush() { flushed = true; }
void glDrawElements(int, unsigned, int, const void*);
void glDrawArrays(int, int, unsigned);
'''
test = r'''
using Triangle = std::array<unsigned char, 3*36>;
static std::vector<TerrainVertex> drawn;
static unsigned byIndex, byCopy;      // vertices sent as indices into the buffer, as copies
static void capture(unsigned index) {
 assert(pointers[0]==active_vertices && pointers[1]==active_vertices+8 && pointers[2]==active_vertices+12);
 TerrainVertex v;
 std::memcpy(&v, active_vertices+index*36, 36);
 // Nothing reaches past near, far or the guard band: there the GE would drop it.
 for (const auto& p: guards) assert(plane_distance(v,p) <= 2e-5f);
 for (float x: v.position) assert(std::isfinite(x));
 // Attributes must be interpolated, not reset at the clipped edge.
 assert(std::fabs(v.uv[0]-(v.position[0]+2)*.25f)<2e-5f);
 assert(std::fabs(v.uv[1]-(v.position[1]+2)*.25f)<2e-5f);
 assert(v.normal[0]==0 && v.normal[1]==0 && v.normal[2]==1);
 assert(v.color[0]==120 && v.color[1]==80 && v.color[2]==40);
 assert(std::fabs(v.color[3]-(126+v.position[0]*8)) < 4);
 drawn.push_back(v);
}
void glDrawElements(int mode, unsigned count, int type, const void* p) {
 // Indices only ever go into the course's own buffer, sixteen bits each.
 assert(mode==GL_TRIANGLES && type==GL_UNSIGNED_SHORT && count%3==0 && count==announced);
 assert(boundBuffer && boundBuffer==liveBuffer && active_vertices==bufferMemory && !byCopy);
 const GLushort* indices=static_cast<const GLushort*>(p);
 for(unsigned i=0;i<count;++i) {
  assert((indices[i]+1)*36L <= bufferBytes);
  capture(indices[i]);
 }
 byIndex+=count;flushed=false;
}
void glDrawArrays(int mode, int first, unsigned count) {
 assert(mode==GL_TRIANGLES && !boundBuffer && count%3==0 && count==announced);
 for(unsigned i=0;i<count;++i) capture(first+i);
 byCopy+=count;flushed=false;
}
static TerrainVertex make_vertex(float x,float y,float z) {
 TerrainVertex v={};
 v.position[0]=x;v.position[1]=y;v.position[2]=z;
 v.uv[0]=(x+2)*.25f;v.uv[1]=(y+2)*.25f;
 v.normal[2]=1;v.color[0]=120;v.color[1]=80;v.color[2]=40;v.color[3]=(GLubyte)(126+x*8);
 return v;
}
static unsigned outcode(const TerrainVertex& v,const TPlane* set) {
 unsigned code=0;
 for(int p=0;p<6;++p) if(plane_distance(v,set[p])>0) code|=1u<<p;
 return code;
}
// The former way: copy every triangle not rejected, cut every one that
// touches a plane of the view itself.
static std::vector<TerrainVertex> former(const TerrainVertex* vertices,const std::vector<GLuint>& indices) {
 std::vector<TerrainVertex> out;
 for(std::size_t i=0;i<indices.size();i+=3) {
  TerrainVertex polygon[12];unsigned codes[3];
  for(int j=0;j<3;++j){polygon[j]=vertices[indices[i+j]];codes[j]=outcode(polygon[j],planes);}
  if(codes[0]&codes[1]&codes[2]) continue;
  const unsigned boundary=codes[0]|codes[1]|codes[2];
  int count=boundary ? clip_polygon(polygon,3,planes,boundary,terrain_lerp) : 3;
  for(int j=1;j+1<count;++j){out.push_back(polygon[0]);out.push_back(polygon[j]);out.push_back(polygon[j+1]);}
 }
 return out;
}
// What of a set of triangles lies inside the view: its area, as a vector.
struct Area { double v[3]; };
static Area visible(const std::vector<TerrainVertex>& triangles) {
 Area sum={};
 for(std::size_t i=0;i<triangles.size();i+=3) {
  TerrainVertex polygon[12]={triangles[i],triangles[i+1],triangles[i+2]};
  int count=clip_polygon(polygon,3,planes,63,terrain_lerp);
  for(int j=1;j+1<count;++j) {
   const float* a=polygon[0].position;const float* b=polygon[j].position;const float* c=polygon[j+1].position;
   double u[3],w[3];
   for(int k=0;k<3;++k){u[k]=b[k]-a[k];w[k]=c[k]-a[k];}
   sum.v[0]+=.5*(u[1]*w[2]-u[2]*w[1]);sum.v[1]+=.5*(u[2]*w[0]-u[0]*w[2]);sum.v[2]+=.5*(u[0]*w[1]-u[1]*w[0]);
  }
 }
 return sum;
}
static std::vector<Triangle> triangles_of(const std::vector<TerrainVertex>& vertices) {
 std::vector<Triangle> out(vertices.size()/3);
 if(!out.empty()) std::memcpy(out.data(),vertices.data(),out.size()*sizeof(Triangle));
 std::sort(out.begin(),out.end());
 return out;
}
static bool useBuffer;
// One frame's worth for one material: `list` through the plane tests,
// `inside` (may be empty) from squares known to lie wholly inside the view.
static void draw(const std::vector<TerrainVertex>& vertices,int rows,int columns,
                 std::vector<GLuint> list,const std::vector<GLuint>& inside,bool expectBuffer) {
 assert(int(vertices.size())<=rows*columns);
 std::vector<TerrainVertex> local;
 if(useBuffer) {
  std::memcpy(bufferMemory,vertices.data(),vertices.size()*36);
  VNCArray=bufferMemory;
 } else {
  local=vertices;VNCArray=reinterpret_cast<GLubyte*>(local.data());
 }
 quadsquare::RowSize=columns;quadsquare::NumRows=rows;
 terrain_pointers(VNCArray);
 quadsquare::VertexArrayIndices=list.data();quadsquare::VertexArrayCounter=list.size();
 insideTriangles=inside.empty()?nullptr:&inside;
 drawn.clear();byIndex=byCopy=0;flushed=false;
 for(auto& sum:profileSums) sum=0;
 quadsquare::DrawTris();
 // The arrays in memory are current again, no buffer left bound, GE started.
 assert(active_vertices==VNCArray && !boundBuffer && insideTriangles==nullptr);
 assert(drawn.size()%3==0 && drawn.size()==byIndex+byCopy);
 assert(drawn.empty() || flushed);

 // The same decisions, made here: rejected by the view's planes, cut only
 // at near, far or beyond the guard band, sent whole otherwise.
 std::vector<TerrainVertex> whole;unsigned cut=0;
 for(GLuint index:inside) whole.push_back(vertices[index]);
 std::vector<TerrainVertex> exact; // wholly inside the view: the former way sent these verbatim
 for(GLuint index:inside) exact.push_back(vertices[index]);
 for(std::size_t i=0;i<list.size();i+=3) {
  unsigned real[3],guard=0;
  for(int j=0;j<3;++j){real[j]=outcode(vertices[list[i+j]],planes);guard|=outcode(vertices[list[i+j]],guards);}
  if(real[0]&real[1]&real[2]) continue;
  if(guard) { ++cut; continue; }
  for(int j=0;j<3;++j) whole.push_back(vertices[list[i+j]]);
  if(!(real[0]|real[1]|real[2])) for(int j=0;j<3;++j) exact.push_back(vertices[list[i+j]]);
 }
 if(expectBuffer) assert(byIndex==whole.size());
 else assert(byIndex==0 && byCopy>=whole.size());
 // Every whole triangle arrives byte for byte, whichever way it is sent.
 const auto sent=triangles_of(drawn);
 const auto wholeTriangles=triangles_of(whole);
 assert(std::includes(sent.begin(),sent.end(),wholeTriangles.begin(),wholeTriangles.end()));
 if(expectBuffer) {
  std::vector<TerrainVertex> first(drawn.begin(),drawn.begin()+byIndex);
  assert(triangles_of(first)==wholeTriangles);
 }
 if(!cut) assert(sent==wholeTriangles);
 if(profiling) {
  assert(profileSums[PSP_N_TRI_INSIDE]==inside.size()/3);
  assert(profileSums[PSP_N_TRI_BOUNDARY]==list.size()/3);
  assert(profileSums[PSP_N_TRI_CLIPPED]==cut);
 }
 // Against the former way: identical for what lies wholly inside the view,
 // and the same part of the view covered for everything else.
 std::vector<GLuint> all(inside);all.insert(all.end(),list.begin(),list.end());
 const auto before=former(vertices.data(),all);
 const auto exactTriangles=triangles_of(exact);
 const auto beforeTriangles=triangles_of(before);
 assert(std::includes(beforeTriangles.begin(),beforeTriangles.end(),exactTriangles.begin(),exactTriangles.end()));
 assert(std::includes(sent.begin(),sent.end(),exactTriangles.begin(),exactTriangles.end()));
 const Area now=visible(drawn),then=visible(before);
 for(int k=0;k<3;++k) assert(std::fabs(now.v[k]-then.v[k])<=2e-3+1e-4*std::fabs(then.v[k]));
}
static void run(float ax,float ay,float bx,float by,float cx,float cy,float az=0,float bz=0,float cz=0) {
 draw({make_vertex(ax,ay,az),make_vertex(bx,by,bz),make_vertex(cx,cy,cz)},1,3,{0,1,2},{},useBuffer);
}
static float area(const std::vector<TerrainVertex>& triangles=drawn) {
 float result=0;
 for(unsigned i=0;i<triangles.size();i+=3) {
  const float* a=triangles[i].position;const float* b=triangles[i+1].position;const float* c=triangles[i+2].position;
  float cross=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
  assert(cross >= -1e-5f); // preserve front-face winding
  result+=cross*.5f;
 }
 return result;
}
// The part of what was drawn that the view shows, by the same measure.
static float seen() {
 std::vector<TerrainVertex> inside;
 for(std::size_t i=0;i<drawn.size();i+=3) {
  TerrainVertex polygon[12]={drawn[i],drawn[i+1],drawn[i+2]};
  int count=clip_polygon(polygon,3,planes,63,terrain_lerp);
  for(int j=1;j+1<count;++j){inside.push_back(polygon[0]);inside.push_back(polygon[j]);inside.push_back(polygon[j+1]);}
 }
 return area(inside);
}
// A sheet of terrain across the view and far beyond it on every side.
static void sheet(float scale,unsigned seed) {
 const int n=28;
 std::vector<TerrainVertex> vertices;
 for(int j=0;j<n;++j)for(int i=0;i<n;++i) {
  const float x=scale*(-6+12.f*i/(n-1)),y=scale*(-6+12.f*j/(n-1));
  vertices.push_back(make_vertex(x,y,.8f*std::sin(x*.9f+seed)*std::cos(y*.6f)+.3f*y));
 }
 std::vector<GLuint> every,list,inside;
 unsigned classes[4]={};
 for(int j=0;j+1<n;++j)for(int i=0;i+1<n;++i) {
  const GLuint a=j*n+i,b=a+1,c=a+n,d=c+1;
  // The square's eight triangle corners: wholly inside the view or not.
  bool within=true;unsigned guard=0,common=63;
  for(GLuint index:{a,b,c,d}) {
   const unsigned code=outcode(vertices[index],planes);
   within&=!code;common&=code;guard|=outcode(vertices[index],guards);
  }
  ++classes[within?0:common?1:guard?2:3];
  for(GLuint index:{a,b,c,b,d,c}){every.push_back(index);(within?inside:list).push_back(index);}
 }
 // Wholly inside, wholly outside, cut, and across a side within the guard band.
 for(unsigned count:classes) assert(count>0);
 const bool buffered=useBuffer;
 draw(vertices,n,n,list,inside,buffered);
 const auto split=triangles_of(drawn);
 // The same triangles through the plane tests alone, and with no list at all.
 draw(vertices,n,n,every,{},buffered);
 assert(triangles_of(drawn)==split);
 draw(vertices,n,n,{},inside,buffered);
 assert(drawn.size()==inside.size() && (buffered ? byIndex : byCopy)==inside.size());
 draw(vertices,n,n,{},{},buffered);
 assert(drawn.empty());
 profiling=false;
 draw(vertices,n,n,list,inside,buffered);
 assert(triangles_of(drawn)==split);
 profiling=true;
 if(buffered) {
  // More vertices than sixteen-bit indices reach, or another detail level:
  // the same triangles, copied.
  draw(vertices,255,257,list,inside,true);
  assert(triangles_of(drawn)==split);
  draw(vertices,256,256,list,inside,false);
  assert(triangles_of(drawn)==split);
  param.perf_level=2;
  draw(vertices,n,n,list,inside,false);
  assert(triangles_of(drawn)==split);
  param.perf_level=1;
 }
}
static void cases() {
 run(0,0,.5f,0,0,.5f);assert(drawn.size()==3 && std::fabs(area()-.125f)<1e-6f);
 // Across near, far and one guard plane: cut at x=-1, x=1 and y=3. What the
 // view shows of it is the square the former clipping left.
 run(-2,-2,4,-2,-2,4);assert(std::fabs(area()-8)<1e-5f && std::fabs(seen()-4)<1e-5f);
 run(2,2,3,2,2,3);assert(drawn.empty());
 // Rejection is by the view's own planes: inside the guard band is not enough.
 run(0,0,.5f,0,0,.5f,2,2,2);assert(drawn.empty());
 run(0,1.5f,.5f,1.5f,0,2.5f);assert(drawn.empty());
 // Across a side plane only, inside the guard band: sent whole, as indices
 // into the buffer or as three copies; the GE cuts it at the screen edge.
 run(0,0,1,0,0,1,0,2,0);
 assert(drawn.size()==3 && (useBuffer ? byIndex : byCopy)==3 && profileSums[PSP_N_TRI_CLIPPED]==0);
 assert(drawn[1].position[2]==2 && std::fabs(area()-.5f)<1e-6f && std::fabs(seen()-.375f)<1e-5f);
 run(0,0,.5f,0,0,2.5f);
 assert(drawn.size()==3 && (useBuffer ? byIndex : byCopy)==3 && profileSums[PSP_N_TRI_CLIPPED]==0);
 assert(drawn[2].position[1]==2.5f && std::fabs(area()-.625f)<1e-6f && std::fabs(seen()-.4f)<1e-5f);
 // Past the guard band it is cut, at the guard plane and not at the view's.
 run(0,0,.5f,0,0,4);
 assert(byIndex==0 && profileSums[PSP_N_TRI_CLIPPED]==1 && std::fabs(area()-.9375f)<1e-5f && std::fabs(seen()-.4375f)<1e-5f);
 run(0,0,1,0,0,1,0,3.5f,0);
 assert(byIndex==0 && profileSums[PSP_N_TRI_CLIPPED]==1 && std::fabs(seen()-.5f*(1-5/7.f*5/7.f))<1e-5f);
 // Near and far have no guard band.
 run(0,0,1.5f,0,0,.5f);
 assert(byIndex==0 && profileSums[PSP_N_TRI_CLIPPED]==1 && std::fabs(area()-seen())<1e-6f && std::fabs(area()-(.375f-.5f*.5f/3*.5f))<1e-5f);
 run(-1,-1,1,-1,-1,1);assert(drawn.size()==3 && std::fabs(area()-2)<1e-5f);
 std::mt19937 rng(42);std::uniform_real_distribution<float> d(-8,8);
 for(int i=0;i<10000;++i) run(d(rng),d(rng),d(rng),d(rng),d(rng),d(rng),d(rng),d(rng),d(rng));
 // Whole sheets, then the view moved: no vertex keeps its old plane tests.
 for(unsigned seed=1;seed<=6;++seed) {
  sheet(1,seed);
  for(auto* set:{planes,guards}) for(int p=0;p<6;++p) set[p].d*=2;
  sheet(1.5f,seed);
  for(auto* set:{planes,guards}) for(int p=0;p<6;++p) set[p].d*=.5f;
  run(0,0,.5f,0,0,.5f);assert(drawn.size()==3);
 }
}
int main() {
 reserve_buffer_memory();
 // The one-pass opaque buckets must exactly match the original repeated
 // per-material selection for every combination of three terrain corners.
 opaqueTerrainIndices.assign(8,{});insideTerrainIndices.assign(8,{});
 VertexIndices[0]=0;VertexIndices[1]=1;VertexIndices[2]=2;
 for(int a=0;a<8;++a)for(int b=0;b<8;++b)for(int c=0;c<8;++c) {
  GLubyte vertices[3*STRIDE_GL_ARRAY]={};GLuint indices[3]={};
  VNCArray=vertices;quadsquare::VertexArrayIndices=indices;
  VertexTerrains[0]=a;VertexTerrains[1]=b;VertexTerrains[2]=c;
  for(auto& bucket:opaqueTerrainIndices)bucket.clear();
  for(auto& bucket:insideTerrainIndices)bucket.clear();
  squareInside=false;
  quadsquare::MakeNoBlendTri(0,1,2,-2);
  for(const auto& bucket:insideTerrainIndices)assert(bucket.empty());
  for(int material=0;material<8;++material) {
   quadsquare::VertexArrayCounter=0;
   quadsquare::MakeNoBlendTri(0,1,2,material);
   assert(opaqueTerrainIndices[material].size()==quadsquare::VertexArrayCounter);
   for(unsigned i=0;i<quadsquare::VertexArrayCounter;++i)
    assert(opaqueTerrainIndices[material][i]==indices[i]);
  }
  // A square wholly inside the view fills the other list, the same way.
  squareInside=true;
  quadsquare::MakeNoBlendTri(0,1,2,-2);
  for(int material=0;material<8;++material) assert(insideTerrainIndices[material]==opaqueTerrainIndices[material]);
  squareInside=false;
 }

 // Without a buffer object every triangle is copied.
 const long bytes=256*257*36;
 assert(!terrainBuffer && !terrain_buffer(bufferMemory,3));
 useBuffer=false;cases();

 // No buffer to be had: null, nothing left behind, still the copying way.
 for(bool* failure:{&failGen,&failData,&failMap}) {
  *failure=true;
  const int deletes=bufferDeletes;
  assert(PspTerrainArrayAlloc(bytes)==nullptr && !terrainBuffer && !boundBuffer && pendingError==GL_NO_ERROR);
  assert(liveBuffer==0 && bufferDeletes==deletes+(failure!=&failGen));
  assert(!terrain_buffer(bufferMemory,3));
  *failure=false;
 }
 run(0,0,.5f,0,0,.5f);assert(byCopy==3 && byIndex==0);

 // The course's array in a buffer object, at its cached address. An error
 // left over from before is not this buffer's.
 pendingError=1;
 GLubyte* array=PspTerrainArrayAlloc(bytes);
 assert(array==bufferMemory && terrainBuffer==liveBuffer && liveBuffer && !boundBuffer && !mapped && bufferBytes==bytes);
 assert(terrain_buffer(array,1) && terrain_buffer(array,65535));
 assert(!terrain_buffer(array,65536) && !terrain_buffer(array,0) && !terrain_buffer(array+36,3));
 GLubyte other[36];
 PspTerrainArrayFilled(nullptr,bytes);PspTerrainArrayFilled(other,36);assert(writebacks==0);
 PspTerrainArrayFilled(array,bytes);assert(writebacks==1 && writtenBack==array && writtenBytes==bytes);
 useBuffer=true;cases();
 // An array that is not the buffer's is copied, buffer or not.
 useBuffer=false;run(0,0,.5f,0,0,.5f);assert(byCopy==3 && byIndex==0);

 // A second course replaces the buffer; freeing gives it back once.
 const int deletes=bufferDeletes;
 assert(PspTerrainArrayAlloc(bytes)==array && bufferDeletes==deletes+1 && liveBuffer);
 assert(!PspTerrainArrayFree(nullptr) && !PspTerrainArrayFree(other) && liveBuffer && terrain_buffer(array,3));
 assert(PspTerrainArrayFree(array) && !liveBuffer && !terrainBuffer && bufferDeletes==deletes+2);
 assert(!PspTerrainArrayFree(array) && !terrain_buffer(array,3));
 std::memset(bufferMemory,0,36*3);
 useBuffer=false;VNCArray=bufferMemory;run(0,0,.5f,0,0,.5f);assert(byCopy==3 && byIndex==0);

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
 puts("PASS: terrain clipping at near, far and the guard band, rejection, winding, area, UV/color/normal interpolation, "
      "buffer-object indices and copies equal to the former clipping, and 2 x 10000 boundary cases");
}
'''
with tempfile.TemporaryDirectory(prefix='etr-terrain-') as tmp:
    directory = Path(tmp)
    cpp = directory / 'test.cpp'
    for header in ('clip_polygon.h', 'psp_profile.h'):
        (directory / header).write_bytes((root / 'ports/extremetuxracer/src' / header).read_bytes())
    (directory / 'view.h').write_text('// TPlane supplied by the test harness.\n')
    cpp.write_text(shim + '\n#include "clip_polygon.h"\n' + implementation + test)
    executable = directory / 'test'
    subprocess.run(['g++', '-std=c++17', '-O2', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', str(cpp), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
