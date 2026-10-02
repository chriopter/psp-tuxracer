#!/usr/bin/env python3
"""Run the computer penguins of the PSP port on the host: pacing, lines, collisions, places."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
src = root/'ports/extremetuxracer/src'
stubs = r'''
#pragma once
#include <cmath>
#include <string>
#include <vector>
struct TVector3d {
 float x=0,y=0,z=0; TVector3d(){} TVector3d(float a,float b,float c):x(a),y(b),z(c){}
 float Length() const {return std::sqrt(x*x+y*y+z*z);} void Norm(){float l=Length(); if(l>0){x/=l;y/=l;z/=l;}}
};
inline TVector3d operator+(TVector3d a,TVector3d b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline TVector3d operator-(TVector3d a,TVector3d b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline TVector3d operator*(float f,TVector3d a){return {a.x*f,a.y*f,a.z*f};}
struct TVector2d {float x,y;};
template<int A,int B> struct TMatrix { TMatrix(){} TMatrix(TVector3d,TVector3d,TVector3d){} void SetTranslationMatrix(float,float,float){} };
template<int A,int B> TMatrix<A,B> operator*(const TMatrix<A,B>& l,const TMatrix<A,B>&){return l;}
inline TVector3d ProjectToPlane(TVector3d,TVector3d v){return v;}
inline TVector3d CrossProduct(TVector3d a,TVector3d b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
struct CCharShape { int drawn=0; void DrawBaked(const TMatrix<4,4>&){++drawn;} };
struct TCharacter { CCharShape* shape=nullptr; };
struct CCharacter { std::vector<TCharacter> CharList; int ensured=0; void Ensure(TCharacter&){++ensured;} };
extern CCharacter Char;
struct CControl { TVector3d cpos, cvel; };
struct TGame { TCharacter* character=nullptr; bool finish=false; };
extern TGame g_game;
struct TCollidable { TVector3d pt; float diam; };
struct TTerrType { float friction; };
struct CCourse {
 std::vector<TCollidable> CollArr; std::vector<TTerrType> TerrList{{0.35f},{0.7f}};
 float rock_from=1e9f, rock_to=1e9f;                      // x range of a strip of rock
 float FindYCoord(float,float z) const {return 0.3f*z;}    // an even slope, z falling downhill
 TVector3d FindCourseNormal(float,float) const {return {0,1,0};}
 TVector2d GetDimensions() const {return {100,1000};}
 TVector2d GetPlayDimensions() const {return {90,900};}
 int GetTerrainIdx(float x,float,float) const {return x>=rock_from&&x<=rock_to?1:0;}
};
extern CCourse Course;
struct CSound { int hits=0; void Play(const char*,int){++hits;} };
extern CSound Sound;
enum clip_result_t {NotVisible,SomeClip,NoClip};
inline clip_result_t clip_aabb_to_view_frustum(TVector3d,TVector3d){return NoClip;}
constexpr float TUX_Y_CORR=0.36f;
'''
test = r'''
#include <cassert>
#include <cstdio>
CCharacter Char; TGame g_game; CCourse Course; CSound Sound;
bool PspFixedStep(){return false;}
const std::vector<unsigned>& ObjectsNear(bool,float z,float reach){
 static std::vector<unsigned> out; out.clear();
 for(unsigned i=0;i<Course.CollArr.size();++i) if(std::fabs(Course.CollArr[i].pt.z-z)<=reach+1) out.push_back(i);
 return out;
}
using namespace Opponents;
static CControl player;
static void run(float seconds,float speed){
 for(int i=0;i<int(seconds*60);++i){
  player.cvel=TVector3d(0,0,-speed);
  player.cpos.z+=player.cvel.z/60; player.cpos.y=Course.FindYCoord(player.cpos.x,player.cpos.z);
  Update(1.f/60,&player);
  for(int k=0;k<racing;++k){ assert(std::isfinite(racers[k].x)&&std::isfinite(racers[k].z)&&std::isfinite(racers[k].speed));
   assert(racers[k].x>=1.5f&&racers[k].x<=98.5f); }
 }
}
static float lead(int i){return player.cpos.z-racers[i].z;}
static void begin(int n){ g_game.finish=false; enabled=n>0; count=n?n:1; player.cpos=TVector3d(50,-3,-10); player.cvel=TVector3d(); Course.CollArr.clear(); Course.rock_from=Course.rock_to=1e9f; Start(&player); }
int main(){
 CCharShape shapes[5]; for(auto& s:shapes) Char.CharList.push_back({&s}); g_game.character=&Char.CharList[4];
 // Off: nobody races, no place.
 begin(0); assert(Count()==0&&Place(&player)==0); run(1,15); Draw();
 // Every count from one to five; never more than five.
 for(int n=1;n<=5;++n){begin(n);assert(Count()==n);} enabled=true;count=9;Start(&player);assert(Count()==MAX);
 // A character each, the player's own only when the others are used up.
 begin(4); for(int i=0;i<4;++i){assert(racers[i].shape&&racers[i].shape!=&shapes[4]); for(int j=0;j<i;++j)assert(racers[i].shape!=racers[j].shape);}
 begin(5); int own=0; for(int i=0;i<5;++i) own+=racers[i].shape==&shapes[4]; assert(own==1);
 // An even run: the field stays about the player, some ahead at times, nobody lost.
 begin(5); run(40,15); float nearest=1e9f;
 for(int i=0;i<5;++i){assert(std::fabs(lead(i))<30); nearest=std::fmin(nearest,std::fabs(lead(i)));}
 assert(nearest<12); int place=Place(&player); assert(place>=1&&place<=6);
 // Solid to each other: no two in the same place.
 for(int i=0;i<5;++i)for(int j=0;j<i;++j) assert(std::fabs(racers[i].x-racers[j].x)>=WIDE*0.9f||std::fabs(racers[i].z-racers[j].z)>=LONG*0.9f);
 // The player stands still for twenty seconds: they get away, but wait; then are caught again.
 run(20,0); float furthest=0; for(int i=0;i<5;++i){assert(lead(i)>5); furthest=std::fmax(furthest,lead(i));}
 assert(furthest<110&&Place(&player)==6);
 run(60,18); for(int i=0;i<5;++i) assert(lead(i)<25);
 // A tree in a penguin's way: it goes round.
 begin(1); run(3,15); const float tx=racers[0].x; Course.CollArr.push_back({TVector3d(tx,0,racers[0].z-45),1.5f});
 float closest=1e9f; for(int i=0;i<600;++i){run(1.f/60,15); if(std::fabs(racers[0].z-Course.CollArr[0].pt.z)<0.6f) closest=std::fmin(closest,std::fabs(racers[0].x-tx));}
 assert(closest>1.2f&&closest<1e8f);
 // Rock across its line: it leaves it for the snow beside.
 begin(1); run(3,15); Course.rock_from=racers[0].x-2.5f; Course.rock_to=racers[0].x+2.5f; const float rf=Course.rock_from,rt=Course.rock_to; run(6,15);
 assert(racers[0].x<rf||racers[0].x>rt);
 // The player runs into one from behind: braked, it is pushed on, both shoved apart, and it is heard once.
 begin(1); run(2,12); racers[0].x=player.cpos.x+0.2f; racers[0].z=player.cpos.z-1.f; racers[0].speed=8; Sound.hits=0;
 player.cvel=TVector3d(0,0,-20); const float before=racers[0].speed; Update(1.f/60,&player);
 assert(player.cvel.z>-20&&racers[0].speed>before&&player.cvel.x<0&&Sound.hits==1);
 player.cvel.z=-20; Update(1.f/60,&player); assert(Sound.hits==1);
 // Not while the player flies over it.
 begin(1); run(2,12); racers[0].x=player.cpos.x; racers[0].z=player.cpos.z-0.5f; player.cpos.y+=3; player.cvel=TVector3d(0,0,-20); Update(1.f/60,&player); assert(player.cvel.z==-20);
 // The finish: the place stands from the moment the player is through.
 begin(3); player.cpos.z=-880; Start(&player); run(1.5f,15); place=Place(&player); g_game.finish=true;
 assert(Place(&player)==place); run(3,2); assert(Place(&player)==place);
 for(int i=0;i<3;++i) assert(racers[i].finished&&racers[i].z>-960);
 // The self-driving player: keys only, paddling when slow, round a tree dead ahead.
 begin(0); player.cvel=TVector3d(0,0,-5); int keys=Autopilot(&player); assert(!(keys&~15)&&(keys&4)&&!(keys&3));
 player.cvel=TVector3d(0,0,-20); assert(!(Autopilot(&player)&4));
 Course.CollArr.push_back({TVector3d(player.cpos.x,0,player.cpos.z-12),1.5f}); keys=Autopilot(&player); assert((keys&3)==1||(keys&3)==2);
 puts("PASS: one to five penguins with characters of their own, a field that stays with the player and waits after a fall, lines round trees and rock, solid racers, places and the self-driving player");
}
'''
with tempfile.TemporaryDirectory(prefix='etr-opponents-') as tmp:
    path = Path(tmp)
    (path/'stubs.h').write_text(stubs)
    for name in ('course.h', 'game_ctrl.h', 'physics.h', 'audio.h', 'tux.h', 'view.h', 'ogl.h'):
        (path/name).write_text('#include "stubs.h"\n')
    (path/'opponents.h').write_text((src/'opponents.h').read_text())
    (path/'test.cpp').write_text((src/'opponents.cpp').read_text() + test)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-fsanitize=undefined,address', '-g',
                    str(path/'test.cpp'), '-o', str(path/'test')], check=True)
    subprocess.run([str(path/'test')], check=True)
