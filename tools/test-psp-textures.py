#!/usr/bin/env python3
"""Exercise production HUD/object batches and terrain/object mipmaps on the host."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
textures = (root/'ports/extremetuxracer/src/textures.cpp').read_text()
digits = textures[textures.index('namespace {\nstruct DigitVertex'):]
platform = (root/'ports/extremetuxracer/psp/platform.cpp').read_text()
mips = platform[platform.index('  const bool mipmapped = '):]
mips = mips[:mips.index('  glTexParameteri(')]
objects=(root/'ports/extremetuxracer/src/course_render.cpp').read_text()
append=objects[objects.index('\tauto append = '):objects.index('\tconst float sine = param.perf_level')]
source = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <cstdio>
using Uint8=uint8_t;
using GLfloat=float;using GLshort=short;using GLuint=unsigned;using GLubyte=uint8_t;
struct TVector3d {float x,y,z;};
struct ObjectVertex {float uv[2],normal[3],position[3];};
void objects_test() {
 std::vector<ObjectVertex> batch;
 // APPEND
 const GLfloat v[]={-2,0,0,2,0,0,2,5,0,-2,5,0};
 const GLshort uv[]={0,1,1,1,1,0,0,0};
 append(v,uv,{10,20,30},{0,0,1});
 assert(batch.size()==6 && batch[0].position[0]==8 && batch[2].position[1]==25);
 assert(batch[5].uv[0]==0 && batch[5].uv[1]==0 && batch[0].normal[2]==1);
 batch.clear();append(v,uv,{10,20,30},{1,0,0},1,0);
 assert(batch[0].position[0]==10 && batch[0].position[2]==32 && batch[1].position[2]==28);
}
namespace sf {struct Color {uint8_t r=255,g=255,b=255,a=255;};}
constexpr int NUMERIC_FONT=0,GL_SRC_ALPHA=1,GL_ONE_MINUS_SRC_ALPHA=2,GL_TEXTURE_2D=3,
 GL_VERTEX_ARRAY=4,GL_TEXTURE_COORD_ARRAY=5,GL_FLOAT=6,GL_TRIANGLES=7,GL_RGB=8,GL_UNSIGNED_SHORT_5_6_5_REV=9,
 GL_COLOR_ARRAY=16,GL_UNSIGNED_BYTE=17,GL_RGBA=10,GL_UNSIGNED_SHORT_4_4_4_4_REV=11,GL_PIXEL_UNPACK_BUFFER_ARB=12,GL_STATIC_DRAW=13,GL_UNPACK_ROW_LENGTH=14,GL_DYNAMIC_DRAW=15,GL_NO_ERROR=0;
bool failAllocation=false;int pendingError=GL_NO_ERROR;
int glGetError(){int result=pendingError;pendingError=GL_NO_ERROR;return result;}
struct {struct {int width=854,height=480;} resolution;} Winsys;
void Message(const char*) {assert(false);}
void glBlendFunc(int,int) {} void glEnable(int) {} void glColor4f(float,float,float,float) {}
const GLubyte* colours;void glColorPointer(int size,int type,unsigned,const void* p) {assert(size==4 && type==GL_UNSIGNED_BYTE);colours=(const GLubyte*)p;}
void glEnableClientState(int) {} void glDisableClientState(int) {}
const float* active; unsigned stride,draws=0,count=0;
void glTexCoordPointer(int,int,unsigned s,const void* p) {active=(const float*)p;stride=s/sizeof(float);}
void glVertexPointer(int size,int,unsigned,const void* p) {assert(size==3 && p==active+3 && (const void*)colours==active+2);}
void glDrawArrays(int,int,unsigned n) {
 ++draws;count=n;
 for(unsigned i=0;i<n;++i) assert(active[i*stride+5]==0 && colours[i*stride*4+1]==200);
}
struct CTexture {bool BindTex(int){return true;} void DrawNumStr(const std::string&,int,int,float,const sf::Color&);void FlushNumStr();};
// DIGITS
unsigned levels=0; std::vector<unsigned> widths;
unsigned bound=0,rowLength=0;std::vector<uint16_t> bufferPixels;
int expectedUsage=GL_STATIC_DRAW;unsigned bufferUploads=0;
void glGenBuffers(int,GLuint* id){*id=1;}
void glBindBuffer(int,GLuint id){bound=id;}
void glBufferData(int,std::size_t size,const void* data,int usage){
 assert(bound==1 && size>=128 && usage==expectedUsage);++bufferUploads;
 if(failAllocation){pendingError=1;return;}
 const auto* p=static_cast<const uint16_t*>(data);bufferPixels.assign(p,p+size/2);
}
void glPixelStorei(int name,unsigned value){assert(name==GL_UNPACK_ROW_LENGTH);rowLength=value;}
void glDeleteBuffers(int,const GLuint*){assert(bound==0 && rowLength==0);}
void glTexImage2D(int,unsigned level,int format,unsigned w,unsigned h,int,int,int,const void* data) {
 assert(!failAllocation); // Never dereference an unallocated PSPGL PBO.
 if(level){assert(level==++levels);widths.push_back(w);}
 else assert(levels==0);
 const auto* p=bound?bufferPixels.data():static_cast<const uint16_t*>(data);
 const unsigned pitch=bound?rowLength:w;
 if(bound)assert(data==nullptr && pitch>=8 && bufferPixels.size()>=pitch*std::max(8u,h));
 const auto expected=format==GL_RGB?uint16_t((128>>3)|((64>>2)<<5)|((32>>3)<<11)):uint16_t(0x8248);
 for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)assert(p[y*pitch+x]==expected);
 if(bound)for(auto pixel:bufferPixels)assert(pixel==expected); // Initialized edge padding, including tiny levels.
}
bool mipmap_test(bool repeated,bool opaque,bool mipmaps=false,unsigned base=256,bool videoMemory=true) {
 (void)repeated;
 unsigned w=base,h=base; std::vector<Uint8> rgba(w*h*4);Uint8* p=rgba.data();
 std::vector<uint16_t> packed(w*h,opaque?uint16_t((128>>3)|((64>>2)<<5)|((32>>3)<<11)):uint16_t(0x8248));
 for(unsigned i=0;i<w*h;++i){p[i*4]=128;p[i*4+1]=64;p[i*4+2]=32;p[i*4+3]=opaque?255:128;}
 levels=0;widths.clear();bufferUploads=0;
 expectedUsage=videoMemory?GL_STATIC_DRAW:GL_DYNAMIC_DRAW;
 // MIPS
 unsigned expected=0;
 if(mipmaps)
  for(unsigned size=base;size>1&&expected<7;size/=2)++expected;
 assert(levels==expected);
 assert(bufferUploads==((mipmaps||!videoMemory)?expected+1:0));
 if(levels) assert(widths.front()==base/2 && widths.back()==(base>>expected) && w==h);
 return true;
}
int main() {
 objects_test();
 // The digits of a frame are one draw, at the flush; each vertex carries its colour.
 CTexture t;t.DrawNumStr("12: 3",11,441,.8f,{10,200,30,255});assert(draws==0);
 t.DrawNumStr("7",100,10,1,{10,200,30,255});t.FlushNumStr();
 assert(draws==1 && count==36 && stride==6);
 assert(active[0]==22.f/256 && active[3]==11 && active[4]==14);
 assert(active[6*6]==44.f/256 && active[6*6+3]==28);
 t.FlushNumStr();assert(draws==1);
 t.DrawNumStr("",0,0,1,{});t.DrawNumStr("?",0,0,1,{});t.FlushNumStr();assert(draws==1);
 mipmap_test(true,true);mipmap_test(false,true);mipmap_test(true,false);mipmap_test(false,false,true);
 mipmap_test(true,true,false,128);mipmap_test(false,false,true,64);mipmap_test(false,true,true,512);
 mipmap_test(false,true,false,256,false);mipmap_test(false,false,false,512,false);
 failAllocation=true;
 assert(!mipmap_test(false,true,false,512,false));
 assert(bound==0 && rowLength==0 && pendingError==GL_NO_ERROR);
 puts("PASS: native HUD/object batches, glyph UVs, empty strings and RGB565/RGBA4444 mipmaps");
}
'''.replace('// DIGITS',digits).replace('// MIPS',mips).replace('// APPEND',append)
with tempfile.TemporaryDirectory(prefix='etr-textures-') as tmp:
    path=Path(tmp)
    (path/'test.cpp').write_text(source)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-fsanitize=undefined',str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)

# Snow tracks must remain present at the default PSP detail level, without
# connecting across jumps or non-snow surfaces. Compile the original routines,
# with the original view frustum (real and guard planes, box test) around them.
tracks=(root/'ports/extremetuxracer/src/track_marks.cpp').read_text()
tracks=tracks[tracks.index('#define TRACK_WIDTH'):]
clip=(root/'ports/extremetuxracer/src/clip_polygon.h').read_text()
clip=clip[clip.index('template<class Vertex>'):clip.rindex('#endif')]
view=(root/'ports/extremetuxracer/src/view.cpp').read_text()
view=view[view.index('static TPlane frustum_planes[6];'):]
profile=(root/'ports/extremetuxracer/src/psp_profile.h').read_text()
tracks_test=r"""
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <list>
#include <random>
#include <set>
#include <vector>
// PROFILE
static unsigned long long profileSums[PSP_SUB_COUNT];
bool PspProfileActive(){return true;}
unsigned long long PspProfileNow(){return 1;}
void PspProfileAdd(unsigned slot,unsigned long long n){assert(slot<PSP_SUB_COUNT);profileSums[slot]+=n;}
struct TVector2d {float x,y;TVector2d(float x=0,float y=0):x(x),y(y){}};
struct TVector3d {
 float x,y,z;TVector3d(float x=0,float y=0,float z=0):x(x),y(y),z(z){}
 float Length()const{return std::sqrt(x*x+y*y+z*z);}
 float Norm(){float n=Length();if(n){x/=n;y/=n;z/=n;}return n;}
 TVector3d operator+(TVector3d b)const{return {x+b.x,y+b.y,z+b.z};}
 TVector3d operator-(TVector3d b)const{return {x-b.x,y-b.y,z-b.z};}
};
TVector3d operator*(float a,TVector3d b){return {a*b.x,a*b.y,a*b.z};}
TVector3d CrossProduct(TVector3d a,TVector3d b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float DotProduct(TVector3d a,TVector3d b){return a.x*b.x+a.y*b.y+a.z*b.z;}
struct TPlane{TVector3d nml;float d;TPlane(float x=0,float y=0,float z=0,float d=0):nml(x,y,z),d(d){}};
float DistanceToPlane(TPlane,TVector3d p){return p.y;}
// The camera's frame in the world: where it is and where its axes point.
struct TMatrix{TVector3d right,up,back,position;};
TVector3d TransformVector(const TMatrix&m,TVector3d v){return v.x*m.right+v.y*m.up+v.z*m.back;}
TVector3d TransformPoint(const TMatrix&m,TVector3d v){return TransformVector(m,v)+m.position;}
struct CControl{TVector3d cpos,cvel,cdirection,viewpos;TMatrix view_mat;};
enum clip_result_t{NoClip,SomeClip,NotVisible};
#define NEAR_CLIP_DIST 0.1
#define ANGLES_TO_RADIANS(x) (M_PI / 180.0 * (x))
struct{struct{int width=480,height=272;}resolution;}Winsys;
struct{int perf_level=1;float fov=60,forward_clip_distance=75;}param;
// CLIP
// VIEW
struct TTerrType {bool trackmarks=true;int starttex=1,tracktex=2,stoptex=3;};
struct {TTerrType TerrList[2];int terrain=1;
 int GetTerrainIdx(float,float,float){return terrain;}
 int drawn=1; int GetDrawnTerrainIdx(float,float){return drawn;}   // what the triangle is drawn as
 float FindYCoord(float,float){return 0;}
 TPlane GetLocalCoursePlane(TVector3d){return {};}
 TVector3d FindCourseNormal(float,float){return {0,1,0};}
} Course;
static CControl camera;
static struct{CControl*ctrl=&camera;}cameraPlayer;
struct{float time_step=1.f/60;decltype(cameraPlayer)*player=&cameraPlayer;}g_game;
namespace sf{struct Color{uint8_t r=255,g=255,b=255,a=255;};}
sf::Color colWhite,colBlack;
void set_material(sf::Color,sf::Color,float){}void set_material_diffuse(sf::Color){}
constexpr int TRACK_MARKS=0,GL_TEXTURE_ENV=1,GL_TEXTURE_ENV_MODE=2,GL_MODULATE=3,GL_QUADS=4,GL_QUAD_STRIP=5;
struct ScopedRenderMode{ScopedRenderMode(int){}};
struct TTexture{void Bind(){}};
struct{TTexture texture;TTexture*GetTexture(int){return &texture;}}Tex;
unsigned drawnVertices=0;
using GLubyte=uint8_t;
constexpr int GL_TEXTURE_COORD_ARRAY=6,GL_COLOR_ARRAY=7,GL_NORMAL_ARRAY=8,GL_VERTEX_ARRAY=9,GL_FLOAT=10,GL_UNSIGNED_BYTE=11,GL_TRIANGLES=12;
void glEnableClientState(int){}void glDisableClientState(int){}
const float* activeUV;
void glTexCoordPointer(int,int,unsigned stride,const void*p){assert(stride==36);activeUV=(const float*)p;}
void glColorPointer(int,int,unsigned,const void*){}
void glNormalPointer(int,unsigned,const void*){}
const float* activePositions;
void glVertexPointer(int,int,unsigned stride,const void*p){assert(stride==36);activePositions=(const float*)p;}
// A cube stands in for the frustum in the first part: planes 0 and 1 are cut
// exactly (near and far), the other four only beyond the wider guard planes.
static void set_cube(float half,float guard){
 const float n[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
 for(int i=0;i<6;++i){
  frustum_planes[i]=TPlane(n[i][0],n[i][1],n[i][2],-half);
  guard_planes[i]=TPlane(n[i][0],n[i][1],n[i][2],i<2?-half:-guard);
  p_vertex_code[i]=(n[i][0]>0?4:0)|(n[i][1]>0?2:0)|(n[i][2]>0?1:0);
 }
}
static float planeTolerance=1e-4f;
static float drawnMaxZ;
static std::vector<float> drawnV;   // per triangle: the middle of its texture v range
void glDrawArrays(int,int,unsigned count){
 assert(count%3==0 && activeUV==activePositions-6);
 for(unsigned i=0;i<count;++i){
  const float*p=activePositions+i*9;
  const auto*color=reinterpret_cast<const uint8_t*>(p)-16;
  assert(color[0]==255 && color[1]==255 && color[2]==255 && color[3]==127);
  assert(std::fabs(p[1]-.08f)<1e-6f);
  drawnMaxZ=std::max(drawnMaxZ,p[2]);
  // Cut exactly at near and far; at a side nothing reaches past the guard band.
  for(const auto&plane:guard_planes)assert(p[0]*plane.nml.x+p[1]*plane.nml.y+p[2]*plane.nml.z+plane.d<planeTolerance);
 }
 for(unsigned i=0;i<count;i+=3){
  float low=1e30f,high=-1e30f;
  for(unsigned j=0;j<3;++j){float v=activeUV[(i+j)*9+1];low=std::min(low,v);high=std::max(high,v);}
  if(high-low>1e-3f)drawnV.push_back(.5f*(low+high));
 }
 drawnVertices+=count;
}
void glTexEnvf(int,int,int){}void glBegin(int){}void glEnd(){}
void glNormal3(TVector3d){}void glTexCoord2(TVector2d){}
void glVertex3(TVector3d v){assert(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z));++drawnVertices;}
// TRACKS
static bool outside(const TPlane&p,TVector3d v){return v.x*p.nml.x+v.y*p.nml.y+v.z*p.nml.z+p.d>0;}
// What chunks and keys must not change: a mark is drawn unless all four of
// its corners lie outside one of the six real planes.
static std::set<const track_quad_t*> brute_force(){
 std::set<const track_quad_t*> seen;
 for(const auto&q:track_marks.quads){
  if(!q.alpha)continue;
  bool rejected=false;
  for(int p=0;p<6&&!rejected;++p)
   rejected=outside(frustum_planes[p],q.v1)&&outside(frustum_planes[p],q.v2)&&
            outside(frustum_planes[p],q.v3)&&outside(frustum_planes[p],q.v4);
  if(!rejected)seen.insert(&q);
 }
 return seen;
}
// The marks of one unbroken track have consecutive texture v ranges: every
// triangle drawn, cut or not, lies inside the range of the mark it came from.
static std::set<const track_quad_t*> drawn_marks(){
 std::vector<std::pair<float,const track_quad_t*>> ranges;
 for(const auto&q:track_marks.quads){assert(q.t3.y>q.t1.y);ranges.push_back({q.t1.y,&q});}
 std::sort(ranges.begin(),ranges.end());
 for(std::size_t i=1;i<ranges.size();++i)assert(ranges[i-1].second->t3.y<=ranges[i].first);
 std::set<const track_quad_t*> seen;
 for(float v:drawnV){
  auto it=std::upper_bound(ranges.begin(),ranges.end(),std::make_pair(v,(const track_quad_t*)nullptr),
                           [](const auto&a,const auto&b){return a.first<b.first;});
  assert(it!=ranges.begin());--it;
  assert(v>it->first && v<it->second->t3.y);
  seen.insert(it->second);
 }
 return seen;
}
static void look(TVector3d eye,float yaw,float pitch){
 // Camera looks down its negative z axis, as the frustum planes assume.
 TVector3d forward(std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch));
 TVector3d right=CrossProduct(forward,TVector3d(0,1,0));right.Norm();
 camera.view_mat={right,CrossProduct(right,forward),-1.f*forward,eye};
 camera.viewpos=eye;
 SetupViewFrustum(&camera);
}
static unsigned compare(){
 drawnV.clear();drawnVertices=0;drawnMaxZ=-1e30f;profileSums[PSP_N_TRACK_DRAWN]=profileSums[PSP_N_TRACK_SEEN]=0;
 DrawTrackmarks();
 const auto expected=brute_force();
 assert(drawn_marks()==expected);
 assert(profileSums[PSP_N_TRACK_DRAWN]==expected.size());
 assert(profileSums[PSP_N_TRACK_SEEN]<=track_marks.quads.size());
 assert(drawnVertices>=6*expected.size());
 return expected.size();
}
// The keys and boxes drawing relies on describe the marks as they are now.
static void check_index(){
 assert(track_keys.size()==track_marks.quads.size());
 assert(track_chunks.size()==(track_keys.size()+TRACK_CHUNK-1)/TRACK_CHUNK);
 std::size_t i=0;
 for(const auto&q:track_marks.quads){
  const auto&key=track_keys[i];const auto&chunk=track_chunks[i/TRACK_CHUNK];
  assert(key.quad==&q && key.x==q.v1.x && key.y==q.v1.y && key.z==q.v1.z);
  for(const auto&v:{q.v1,q.v2,q.v3,q.v4}){
   assert(v.x>=chunk.min[0] && v.x<=chunk.max[0] && v.y>=chunk.min[1] && v.y<=chunk.max[1]);
   assert(v.z>=chunk.min[2] && v.z<=chunk.max[2]);
   assert((v-q.v1).Length()<TRACK_REACH);   // what the first-corner shortcut assumes
  }
  if(&q==&*track_marks.current_mark)assert(i==track_index);
  ++i;
 }
 std::size_t total=0;
 for(const auto&chunk:track_chunks){assert(chunk.count>0 && chunk.count<=TRACK_CHUNK);total+=chunk.count;}
 assert(total==track_keys.size());
}
int main(){
 set_cube(10,30);
 init_track_marks();DrawTrackmarks();assert(drawnVertices==0);
 CControl ctrl{{0,0,0},{0,0,-10},{0,0,-1},{},{}};
 UpdateTrackmarks(&ctrl);ctrl.cpos.z-=.2f;UpdateTrackmarks(&ctrl);
 assert(track_marks.quads.size()==2 && continuing_track);
 assert(track_marks.quads.front().track_type==TRACK_HEAD);
 assert(track_marks.quads.back().track_type==TRACK_TAIL);
 const auto& q=track_marks.quads.back();
 assert(std::fabs((q.v4-q.v3).Length()-.7f)<1e-6f);
 assert(std::fabs(q.v3.y-.08f)<1e-6f && q.alpha>0);
 check_index();
 DrawTrackmarks();assert(drawnVertices>=8);
 assert(compare()==2 && drawnVertices==12);
 drawnVertices=0;frustum_planes[4].d=1;DrawTrackmarks();assert(drawnVertices==0);frustum_planes[4].d=-10;
 // Across the near plane a mark is cut (the draw call checks every vertex).
 drawnVertices=0;frustum_planes[0].d=guard_planes[0].d=0;DrawTrackmarks();assert(drawnVertices>0);
 assert(compare()==2 && drawnVertices==18);
 set_cube(10,30);
 // Across a side plane, but inside the guard band: drawn whole, uncut.
 frustum_planes[4].d=.1f;
 assert(compare()==1 && drawnVertices==6 && drawnMaxZ==0);
 // Past the guard band it is cut, at the guard plane and not at the view's:
 // one triangle of the mark becomes a quad, the other a smaller triangle.
 guard_planes[4].d=.05f;
 assert(compare()==1 && drawnVertices==9 && std::fabs(drawnMaxZ+.05f)<1e-6f);
 set_cube(10,30);
 // Beyond the sphere around the eye nothing is drawn, whatever the planes say.
 camera.viewpos={0,0,500};drawnVertices=0;DrawTrackmarks();assert(drawnVertices==0);
 camera.viewpos={0,0,0};
 ctrl.cpos.y=.5f;UpdateTrackmarks(&ctrl);
 assert(!continuing_track && track_marks.quads.size()==2);
 ctrl.cpos.y=0;Course.TerrList[1].trackmarks=false;UpdateTrackmarks(&ctrl);
 assert(!continuing_track && track_marks.quads.size()==2);
 Course.TerrList[1].trackmarks=true;UpdateTrackmarks(&ctrl);
 assert(track_marks.quads.back().track_type==TRACK_HEAD);
 // Snow by the weights, but drawn as ice (terrain 0, no marks): no trench on what one sees as ice.
 { const std::size_t before=track_marks.quads.size(); Course.TerrList[0].trackmarks=false; Course.drawn=0;
   ctrl.cpos.z-=.2f;UpdateTrackmarks(&ctrl); assert(!continuing_track && track_marks.quads.size()==before);
   Course.drawn=1; ctrl.cpos.z-=.2f;UpdateTrackmarks(&ctrl); assert(track_marks.quads.size()==before+1);
   Course.TerrList[0].trackmarks=true; }
 Course.terrain=-1;UpdateTrackmarks(&ctrl);assert(!continuing_track);
 Course.terrain=1;
 for(unsigned i=0;i<MAX_TRACK_MARKS+10;++i){ctrl.cpos.z-=.2f;UpdateTrackmarks(&ctrl);}
 assert(track_marks.quads.size()==MAX_TRACK_MARKS);
 check_index();
 init_track_marks();assert(track_marks.quads.empty()&&!continuing_track);
 assert(track_chunks.empty()&&track_keys.empty());

 // The guard planes share near and far with the view and open each side to
 // GUARD_BAND times its tangent.
 look({3,2,1},.7f,-.3f);
 const float tv=std::tan(ANGLES_TO_RADIANS(param.fov*.5f)),th=tv*480/272;
 // Wider than the view, inside the GE's coordinate range (8.5 half-widths).
 assert(GUARD_BAND>1 && GUARD_BAND<=8.5f);
 for(int i=0;i<2;++i)assert(guard_planes[i].d==frustum_planes[i].d && guard_planes[i].nml.x==frustum_planes[i].nml.x);
 for(float side:{-1.f,1.f})for(int axis=0;axis<2;++axis)for(float factor:{.9f,1.1f,GUARD_BAND*.97f,GUARD_BAND*1.03f}){
  TVector3d point=TransformPoint(camera.view_mat,axis?TVector3d(0,side*factor*tv*10,-10):TVector3d(side*factor*th*10,0,-10));
  bool inView=true,inGuard=true;
  for(int i=0;i<6;++i){inView&=!outside(frustum_planes[i],point);inGuard&=!outside(guard_planes[i],point);}
  assert(inView==(factor<1) && inGuard==(factor<GUARD_BAND));
 }

 // Marks along a line, seen from many places: chunks and keys draw exactly
 // the marks the plane test alone would, before and after the ring wraps.
 planeTolerance=2e-2f;
 std::mt19937 rng(7);
 std::uniform_real_distribution<float> unit(0,1);
 for(float heading:{0.f,.6f,2.2f}){
  init_track_marks();
  const TVector3d direction(std::sin(heading),0,-std::cos(heading));
  ctrl={{5,0,-3},10.f*direction,direction,{},{}};
  unsigned laid=0,drawn=0,empty=0,views=0;
  for(unsigned target:{40u,1000u,unsigned(MAX_TRACK_MARKS),unsigned(MAX_TRACK_MARKS)+700u,2*unsigned(MAX_TRACK_MARKS)+1500u}){
   for(;laid<target;++laid){ctrl.cpos=ctrl.cpos+.2f*direction;UpdateTrackmarks(&ctrl);}
   assert(continuing_track && track_marks.quads.size()==std::min(target,unsigned(MAX_TRACK_MARKS)));
   check_index();
   const unsigned oldest=laid-track_marks.quads.size();
   for(int i=0;i<120;++i){
    // Near the newest mark, near the oldest (the seam of the ring) or anywhere.
    const float pick=unit(rng);
    const float along=.2f*(pick<.3f?laid-unit(rng)*60:pick<.6f?oldest+unit(rng)*60:oldest+unit(rng)*(laid-oldest));
    const TVector3d side=CrossProduct(direction,TVector3d(0,1,0));
    const TVector3d eye=TVector3d(5,0,-3)+along*direction+(unit(rng)*40-20)*side+TVector3d(0,.3f+unit(rng)*12,0);
    const unsigned n=i<8
      ? (look(eye,heading+(i&1?3.14159265f:0)+(i&2?.2f:0),i&4?-.5f:-.05f),compare())
      : (look(eye,unit(rng)*6.2831853f,unit(rng)*2.4f-1.4f),compare());
    drawn+=n;empty+=!n;++views;
   }
  }
  // Not a comparison of nothing with nothing, nor of everything with everything.
  assert(drawn>20000 && empty>views/20 && empty<views*4/5);
 }
 puts("PASS: default-profile snow trench, original width/height/alpha, jump/surface breaks, bounded ring, guard-band clipping and chunk/key culling equal to the plane test");
}
""".replace('// TRACKS',tracks).replace('// CLIP',clip).replace('// VIEW',view).replace('// PROFILE',profile)
with tempfile.TemporaryDirectory(prefix='etr-tracks-') as tmp:
    path=Path(tmp)
    (path/'test.cpp').write_text(tracks_test)
    subprocess.run(['g++','-std=c++17','-O1','-Wall','-Wextra','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)

# Compile the production decode and file-loading methods against an SDL stub.
# Compare every byte with the former full-image-then-nearest-neighbour path.
decode = platform[platform.index('void Image::create('):platform.index('void Image::flipVertically(')]
file_load = platform[platform.index('bool Texture::loadFromFile('):platform.index('bool Texture::loadFromImage(')]
decode_test = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using Uint8=uint8_t; using Uint32=uint32_t;
struct Vector2u {unsigned x=0,y=0;};
struct Color {Uint8 r=0,g=0,b=0,a=255;};
struct SDL_PixelFormat {unsigned BytesPerPixel=4,Rloss=0,Gloss=0,Bloss=0,Aloss=0,Rshift=0,Gshift=8,Bshift=16,Ashift=24,Amask=0xff000000;};
using Format=SDL_PixelFormat;
struct SDL_Surface {int w,h;unsigned pitch;Format* format;void* pixels;};
Format format; SDL_Surface surface;std::vector<Uint8> input;
SDL_Surface* IMG_Load(const char*) {return &surface;}
const char* IMG_GetError(){return "mock";}
void PspTraceResource(const char*,const char*){}
void SDL_LockSurface(SDL_Surface*){} void SDL_UnlockSurface(SDL_Surface*){}
void SDL_FreeSurface(SDL_Surface*){}
void SDL_GetRGBA(Uint32 c,Format*,Uint8*r,Uint8*g,Uint8*b,Uint8*a){
 *r=c;*g=c>>8;*b=c>>16;*a=c>>24;
}
class Image {
 Vector2u size;std::vector<Uint8> pixels;
public:
 void create(unsigned,unsigned,Color c=Color());
 bool loadFromFile(const std::string&);
 bool loadTextureFromFile(const std::string&,unsigned,Vector2u&);
 Vector2u getSize()const{return size;}
 const Uint8* getPixelsPtr()const{return pixels.data();}
};
// DECODE
Vector2u uploaded;
class Texture {
public:
 bool mipmaps=false,videoMemory=false,objectTexture=false;unsigned maxSize=256;Vector2u size;
 bool loadFromFile(const std::string&);
 bool loadFromImage(const Image&i){uploaded=i.getSize();size=uploaded;return true;}
};
// FILE_LOAD
int main(){
 for(auto dims: {Vector2u{512,512},{384,384},{1058,734},{480,272},{7,3},{256,256}}){
  surface={int(dims.x),int(dims.y),dims.x*4+12,&format,nullptr};
  input.resize(surface.pitch*dims.y);
  for(unsigned k=0;k<input.size();++k)input[k]=(k*37+k/13)%256;
  surface.pixels=input.data();
  Image full;assert(full.loadFromFile("mock"));
  assert(full.getSize().x==dims.x && full.getSize().y==dims.y);
  for(unsigned y=0;y<dims.y;++y)
   assert(!memcmp(full.getPixelsPtr()+y*dims.x*4,input.data()+y*surface.pitch,dims.x*4));
  for(unsigned limit:{128u,256u,512u}){
   Image bounded;Vector2u original;
   assert(bounded.loadTextureFromFile("mock",limit,original));
   assert(original.x==dims.x && original.y==dims.y);
   unsigned w=std::min(limit,pot(dims.x)),h=std::min(limit,pot(dims.y));
   assert(bounded.getSize().x==w && bounded.getSize().y==h);
   for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)
    assert(!memcmp(bounded.getPixelsPtr()+(y*w+x)*4,
       full.getPixelsPtr()+((y*dims.y/h)*dims.x+x*dims.x/w)*4,4));
   Texture texture;texture.maxSize=limit;
   assert(texture.loadFromFile("data/objects/mock.png"));
   assert(texture.size.x==dims.x && texture.size.y==dims.y);
   assert(uploaded.x==w && uploaded.y==h);
  }
 }
 puts("PASS: bounded decode preserves every target texel, full image loads and logical sprite dimensions");
}
'''.replace('// DECODE',decode).replace('// FILE_LOAD',file_load)
with tempfile.TemporaryDirectory(prefix='etr-decode-') as tmp:
    path=Path(tmp)
    (path/'test.cpp').write_text(decode_test)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-fsanitize=undefined',str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
