#!/usr/bin/env python3
"""Check shipped UI texture dimensions, production snow batching and the Start gate."""
from pathlib import Path
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
for name, dimensions in [('psp-controls.png', (256, 128)), ('psp-guide.png', (512, 256))]:
    png = (root / 'ports/extremetuxracer/data/textures' / name).read_bytes()
    assert png[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II', png[16:24]) == dimensions

particles = (root / 'ports/extremetuxracer/src/particles.cpp').read_text()
draw = particles[particles.index('void draw_ui_snow()'):particles.index('\nvoid push_ui_snow(')]
guide = (root / 'ports/extremetuxracer/src/controls_guide.cpp').read_text()
gate = guide[guide.index('void CControlsGuide::Keyb('):guide.index('\nvoid CControlsGuide::Loop(')]
source = r'''
#include <cassert>
#include <vector>
#include <iostream>
namespace sf {
struct Vector2f { float x,y; Vector2f(float a=0,float b=0):x(a),y(b){} };
struct Color { int r,g,b,a; Color(int r_=0,int g_=0,int b_=0,int a_=0):r(r_),g(g_),b(b_),a(a_){} };
struct Vertex { Vector2f position; Color color; Vector2f texCoords;
  Vertex(Vector2f p={},Color c={},Vector2f uv={}):position(p),color(c),texCoords(uv){} };
constexpr int Quads=0;
struct VertexArray { std::vector<Vertex> v; VertexArray(int,std::size_t n):v(n){};
  void resize(std::size_t n){v.resize(n);} Vertex& operator[](std::size_t n){return v[n];} };
struct RenderStates { const int* texture=nullptr; };
struct Keyboard { enum Key { P,Return,Escape,Up,Down,Left,Right,Space,T,R }; };
}
struct Rect {int left,top,width,height;};
struct Sprite { sf::Vector2f p,s; Rect r;
  auto getPosition() const {return p;} auto getScale() const{return s;}
  auto getTextureRect() const {return r;} };
struct Particle { Sprite sprite; };
std::vector<Particle> particles_2d;
constexpr int SNOW_PART=1;
struct Textures { int atlas=7; const int& GetSFTexture(int){return atlas;} } Tex;
struct Window { int calls=0; std::vector<sf::Vertex> captured;
  void draw(const sf::VertexArray& a,const sf::RenderStates& s){
    assert(s.texture==&Tex.atlas); captured=a.v; ++calls;
  } } Winsys;
struct State { struct Manager { int requests=0; void RequestEnterState(int){++requests;} }; static Manager manager; };
State::Manager State::manager;
int GameTypeSelect=1;
struct CControlsGuide { void Keyb(sf::Keyboard::Key,bool,int,int); };
// DRAW
// GATE
int main(){
  particles_2d={{{{10,20},{2,3},{16,0,16,8}}},{{{-4,7},{1,1},{0,8,16,8}}}};
  draw_ui_snow(); assert(Winsys.calls==1 && Winsys.captured.size()==8);
  auto v=Winsys.captured;
  assert(v[0].position.x==10 && v[0].position.y==20);
  assert(v[1].position.x==42 && v[1].position.y==20);
  assert(v[2].position.x==42 && v[2].position.y==44);
  assert(v[3].position.x==10 && v[3].position.y==44);
  assert(v[0].texCoords.x==16 && v[0].texCoords.y==0);
  assert(v[2].texCoords.x==32 && v[2].texCoords.y==8);
  assert(v[4].position.x==-4 && v[4].texCoords.y==8);
  for(auto vertex:v) assert(vertex.color.r==255 && vertex.color.a==76);
  particles_2d.resize(4000); draw_ui_snow();
  assert(Winsys.calls==2 && Winsys.captured.size()==16000);
  particles_2d.clear(); draw_ui_snow(); assert(Winsys.captured.empty());
  CControlsGuide guide;
  for(int key=sf::Keyboard::Return; key<=sf::Keyboard::R; ++key)
    guide.Keyb(static_cast<sf::Keyboard::Key>(key),false,0,0);
  guide.Keyb(sf::Keyboard::P,true,0,0); assert(State::manager.requests==0);
  guide.Keyb(sf::Keyboard::P,false,0,0); assert(State::manager.requests==1);
  std::cout<<"PASS: snow geometry/UV/alpha, 4000 flakes in one batch, empty batch and Start-only introduction\n";
}
'''.replace('// DRAW', draw).replace('// GATE', gate)
with tempfile.TemporaryDirectory(prefix='etr-ui-') as directory:
    temp = Path(directory)
    (temp / 'ui.cpp').write_text(source)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-fsanitize=undefined',
                    str(temp / 'ui.cpp'), '-o', str(temp / 'ui')], check=True)
    subprocess.run([str(temp / 'ui')], check=True)
print('PASS: runtime controller and diagram atlas dimensions')
