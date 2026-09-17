#!/usr/bin/env python3
"""Compile the actual contextual PSP bindings and exercise held-button transitions."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
platform = (root / 'ports/extremetuxracer/psp/platform.cpp').read_text()
mapping = platform[platform.index('  bool racing = State::manager.CurrentState() == &Racing;'):]
mapping = mapping[:mapping.index('  for (int i = 0; i < Keyboard::KeyCount; i++)')]
buttons = 'LEFT RIGHT UP DOWN CROSS CIRCLE SQUARE TRIANGLE START LTRIGGER RTRIGGER'.split()
defines = '\n'.join(f'constexpr unsigned PSP_CTRL_{name} = 1u << {i};' for i, name in enumerate(buttons))
source = r'''
#include <array>
#include <cassert>
#include <iostream>
namespace Keyboard { enum Key { Unknown=-1, Left,Right,Up,Down,Space,Return,Escape,T,R,P,KeyCount }; }
int Racing, Paused, Menu, ControlsGuide, Help;
struct State {
  struct Manager { const int* state; const int* CurrentState() { return state; } };
  static Manager manager;
};
State::Manager State::manager;
// DEFINES
std::array<bool,Keyboard::KeyCount> sample(const int* state, unsigned b) {
  State::manager.state=state;
  // MAPPING
  std::array<bool,Keyboard::KeyCount> out{};
  for (int i=0;i<Keyboard::KeyCount;i++) out[i]=now[i];
  return out;
}
void release() { sample(&Menu,0); }
int main() {
  using namespace Keyboard;
  const Key menu[] = {Left,Right,Up,Down,Return,Escape,Unknown,Unknown,Unknown,Unknown,Unknown};
  const Key race[] = {Left,Right,Up,Down,Space,P,T,R,P,Down,Up};
  const Key pause[] = {Left,Right,Up,Down,Return,Escape,Unknown,Unknown,P,Unknown,Unknown};
  for (int context=0;context<3;context++) {
    const int* state=context==0?&Menu:context==1?&Racing:&Paused;
    const Key* expected=context==0?menu:context==1?race:pause;
    for (int b=0;b<11;b++) {
      release(); auto out=sample(state,1u<<b);
      for(int k=0;k<KeyCount;k++) assert(out[k]==(k==expected[b]));
    }
  }
  release(); assert(sample(&Menu,PSP_CTRL_CROSS)[Return]);
  auto held=sample(&Racing,PSP_CTRL_CROSS);
  assert(held[Return] && !held[Space]); // Confirm must not turn into a jump.
  release(); assert(sample(&Racing,PSP_CTRL_CROSS)[Space]);
  release(); assert(sample(&Racing,PSP_CTRL_START)[P]);
  held=sample(&Paused,PSP_CTRL_START);
  assert(held[P] && !held[Return]); // Pause must not confirm an item.
  release(); sample(&Menu,PSP_CTRL_START);
  held=sample(&Racing,PSP_CTRL_START);
  for(bool down:held) assert(!down); // Ignored menu press stays ignored until release.
  release(); sample(&Racing,PSP_CTRL_CIRCLE);
  held=sample(&Paused,PSP_CTRL_CIRCLE); assert(held[P] && !held[Escape]);
  release(); assert(sample(&Paused,PSP_CTRL_CIRCLE)[Escape]);
  release(); held=sample(&Racing,PSP_CTRL_CROSS|PSP_CTRL_SQUARE|PSP_CTRL_LEFT);
  assert(held[Space] && held[T] && held[Left]);
  for (const int* state : {&ControlsGuide, &Help}) {
    for (int b=0;b<11;b++) {
      release(); auto out=sample(state,1u<<b);
      Key expected = b==8 ? P : menu[b];
      for(int k=0;k<KeyCount;k++) assert(out[k]==(k==expected));
    }
  }
  release(); assert(sample(&ControlsGuide,PSP_CTRL_START)[P]);
  held=sample(&Menu,PSP_CTRL_START);
  assert(held[P] && !held[Return]); // Start must not select a menu entry on arrival.
  std::cout << "PASS: 55 bindings, guide Start gate, disabled menu actions, held-state transitions and simultaneous controls\n";
}
'''.replace('// DEFINES', defines).replace('// MAPPING', mapping)
with tempfile.TemporaryDirectory(prefix='etr-controls-') as temp:
    path = Path(temp)
    (path / 'controls.cpp').write_text(source)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-fsanitize=undefined',
                    str(path / 'controls.cpp'), '-o', str(path / 'controls')], check=True)
    subprocess.run([str(path / 'controls')], check=True)
