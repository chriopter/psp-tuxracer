#!/usr/bin/env python3
"""Exercise production HUD/object batches and terrain/object mipmaps on the host."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
textures = (root/'ports/extremetuxracer/src/textures.cpp').read_text()
digits = textures[textures.index('void CTexture::DrawNumStr('):]
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
using GLfloat=float;using GLshort=short;using GLuint=unsigned;
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
namespace sf {struct Color {};}
constexpr int NUMERIC_FONT=0,GL_SRC_ALPHA=1,GL_ONE_MINUS_SRC_ALPHA=2,GL_TEXTURE_2D=3,
 GL_VERTEX_ARRAY=4,GL_TEXTURE_COORD_ARRAY=5,GL_FLOAT=6,GL_TRIANGLES=7,GL_RGB=8,GL_UNSIGNED_SHORT_5_6_5_REV=9,
 GL_RGBA=10,GL_UNSIGNED_SHORT_4_4_4_4_REV=11,GL_PIXEL_UNPACK_BUFFER_ARB=12,GL_STATIC_DRAW=13,GL_UNPACK_ROW_LENGTH=14,GL_DYNAMIC_DRAW=15,GL_NO_ERROR=0;
bool failAllocation=false;int pendingError=GL_NO_ERROR;
int glGetError(){int result=pendingError;pendingError=GL_NO_ERROR;return result;}
struct {struct {int width=854,height=480;} resolution;} Winsys;
void Message(const char*) {assert(false);}
void glBlendFunc(int,int) {} void glEnable(int) {} void glColor(sf::Color) {}
void glEnableClientState(int) {} void glDisableClientState(int) {}
const float* active; unsigned stride,draws=0,count=0;
void glTexCoordPointer(int,int,unsigned s,const void* p) {active=(const float*)p;stride=s/sizeof(float);}
void glVertexPointer(int size,int,unsigned,const void* p) {assert(size==3 && p==active+2);}
void glDrawArrays(int,int,unsigned n) {
 ++draws;count=n;
 for(unsigned i=0;i<n;++i) assert(active[i*stride+4]==0);
}
struct CTexture {bool BindTex(int){return true;} void DrawNumStr(const std::string&,int,int,float,const sf::Color&);};
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
 CTexture t;t.DrawNumStr("12: 3",11,441,.8f,{});
 assert(draws==1 && count==30 && stride==5);
 assert(active[0]==22.f/256 && active[2]==11 && active[3]==14);
 assert(active[5*6]==44.f/256 && active[5*6+2]==28);
 t.DrawNumStr("",0,0,1,{});assert(draws==1);
 t.DrawNumStr("?",0,0,1,{});assert(draws==1);
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
# connecting across jumps or non-snow surfaces. Compile the original routines.
tracks=(root/'ports/extremetuxracer/src/track_marks.cpp').read_text()
tracks=tracks[tracks.index('#define TRACK_WIDTH'):]
clip=(root/'ports/extremetuxracer/src/clip_polygon.h').read_text()
clip=clip[clip.index('template<class Vertex>'):clip.rindex('#endif')]
tracks_test=r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <list>
#include <vector>
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
struct TPlane{TVector3d nml;float d=0;};float DistanceToPlane(TPlane,TVector3d p){return p.y;}
TPlane planes[]={{{1,0,0},-10},{{-1,0,0},-10},{{0,1,0},-10},{{0,-1,0},-10},{{0,0,1},-10},{{0,0,-1},-10}};
const TPlane*get_view_clip_planes(){return planes;}
// CLIP
struct CControl{TVector3d cpos,cvel,cdirection;};
struct TTerrType {bool trackmarks=true;int starttex=1,tracktex=2,stoptex=3;};
struct {TTerrType TerrList[2];int terrain=1;
 int GetTerrainIdx(float,float,float){return terrain;}
 float FindYCoord(float,float){return 0;}
 TPlane GetLocalCoursePlane(TVector3d){return {};}
 TVector3d FindCourseNormal(float,float){return {0,1,0};}
} Course;
struct{int perf_level=1;}param;
struct{float time_step=1.f/60;}g_game;
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
void glTexCoordPointer(int,int,unsigned,const void*){}void glColorPointer(int,int,unsigned,const void*){}
void glNormalPointer(int,unsigned,const void*){}
const float* activePositions;
void glVertexPointer(int,int,unsigned stride,const void*p){assert(stride==36);activePositions=(const float*)p;}
void glDrawArrays(int,int,unsigned count){
 for(unsigned i=0;i<count;++i){
  const float*p=activePositions+i*9;
  const auto*color=reinterpret_cast<const uint8_t*>(p)-16;
  assert(color[0]==255 && color[1]==255 && color[2]==255 && color[3]==127);
  assert(std::fabs(p[1]-.08f)<1e-6f);
  for(const auto&plane:planes)assert(p[0]*plane.nml.x+p[1]*plane.nml.y+p[2]*plane.nml.z+plane.d<1e-4f);
 }
 drawnVertices+=count;
}
void glTexEnvf(int,int,int){}void glBegin(int){}void glEnd(){}
void glNormal3(TVector3d){}void glTexCoord2(TVector2d){}
void glVertex3(TVector3d v){assert(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z));++drawnVertices;}
// TRACKS
int main(){
 init_track_marks();DrawTrackmarks();assert(drawnVertices==0);
 CControl ctrl{{0,0,0},{0,0,-10},{0,0,-1}};
 UpdateTrackmarks(&ctrl);ctrl.cpos.z-=.2f;UpdateTrackmarks(&ctrl);
 assert(track_marks.quads.size()==2 && continuing_track);
 assert(track_marks.quads.front().track_type==TRACK_HEAD);
 assert(track_marks.quads.back().track_type==TRACK_TAIL);
 const auto& q=track_marks.quads.back();
 assert(std::fabs((q.v4-q.v3).Length()-.7f)<1e-6f);
 assert(std::fabs(q.v3.y-.08f)<1e-6f && q.alpha>0);
 DrawTrackmarks();assert(drawnVertices>=8);
 drawnVertices=0;planes[4].d=1;DrawTrackmarks();assert(drawnVertices==0);planes[4].d=-10;
 drawnVertices=0;planes[0].d=0;DrawTrackmarks();assert(drawnVertices>0);planes[0].d=-10;
 ctrl.cpos.y=.5f;UpdateTrackmarks(&ctrl);
 assert(!continuing_track && track_marks.quads.size()==2);
 ctrl.cpos.y=0;Course.TerrList[1].trackmarks=false;UpdateTrackmarks(&ctrl);
 assert(!continuing_track && track_marks.quads.size()==2);
 Course.TerrList[1].trackmarks=true;UpdateTrackmarks(&ctrl);
 assert(track_marks.quads.back().track_type==TRACK_HEAD);
 Course.terrain=-1;UpdateTrackmarks(&ctrl);assert(!continuing_track);
 Course.terrain=1;
 for(unsigned i=0;i<MAX_TRACK_MARKS+10;++i){ctrl.cpos.z-=.2f;UpdateTrackmarks(&ctrl);}
 assert(track_marks.quads.size()==MAX_TRACK_MARKS);
 init_track_marks();assert(track_marks.quads.empty()&&!continuing_track);
 puts("PASS: default-profile snow trench, original width/height/alpha, jump/surface breaks and bounded ring");
}
'''.replace('// TRACKS',tracks).replace('// CLIP',clip)
with tempfile.TemporaryDirectory(prefix='etr-tracks-') as tmp:
    path=Path(tmp)
    (path/'test.cpp').write_text(tracks_test)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-fsanitize=undefined',str(path/'test.cpp'),'-o',str(path/'test')],check=True)
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
struct Format {unsigned BytesPerPixel=4;};
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
 bool mipmaps=false,videoMemory=false;unsigned maxSize=256;Vector2u size;
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
