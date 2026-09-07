// SPDX-License-Identifier: GPL-2.0-or-later
// PSP implementation of the SFML API subset used by Extreme Tux Racer.
#pragma once
#include <GL/gl.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <pspkernel.h>
#include <string>
#include <vector>
namespace sf {
using Uint8 = uint8_t;
using Uint32 = uint32_t;
template <class T> struct Vector2 {
  T x{}, y{};
  Vector2() = default;
  Vector2(T a, T b) : x(a), y(b) {};
  Vector2 operator+(Vector2 b) const { return {x + b.x, y + b.y}; }
  Vector2 operator-(Vector2 b) const { return {x - b.x, y - b.y}; }
};
using Vector2f = Vector2<float>;
using Vector2u = Vector2<unsigned>;
using Vector2i = Vector2<int>;
struct Color {
  Uint8 r, g, b, a;
  constexpr Color(Uint8 r_ = 0, Uint8 g_ = 0, Uint8 b_ = 0, Uint8 a_ = 255)
      : r(r_), g(g_), b(b_), a(a_) {}
  static const Color White, Black, Red, Blue, Yellow, Transparent;
};
inline bool operator==(Color a, Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
inline bool operator!=(Color a, Color b) { return !(a == b); }
struct Time {
  uint64_t us;
  float asSeconds() const { return us / 1000000.f; }
};
class Clock {
  uint64_t start = sceKernelGetSystemTimeWide();

public:
  Time getElapsedTime() const { return {sceKernelGetSystemTimeWide() - start}; }
  Time restart() {
    Time t = getElapsedTime();
    start = sceKernelGetSystemTimeWide();
    return t;
  }
};
class String {
  std::u32string value;

public:
  String &operator+=(const String &s) {
    value += s.value;
    return *this;
  }
  friend String operator+(String a, const String &b) { return a += b; }
  String() = default;
  String(char c) : value(1, c) {}
  String(const char *s) : String(std::string(s)) {}
  String(const std::string &s) {
    for (unsigned char c : s)
      value += c;
  }
  template <class It> static String fromUtf8(It begin, It end) {
    String s;
    while (begin != end) {
      unsigned c = (unsigned char)*begin++;
      int n = 0;
      if (c >= 240) {
        c &= 7;
        n = 3;
      } else if (c >= 224) {
        c &= 15;
        n = 2;
      } else if (c >= 192) {
        c &= 31;
        n = 1;
      }
      while (n-- && begin != end)
        c = (c << 6) | ((unsigned char)*begin++ & 63);
      s.value += c;
    }
    return s;
  }
  operator std::string() const { return toAnsiString(); }
  std::string toAnsiString() const {
    std::string s;
    for (auto c : value)
      s += c < 256 ? (char)c : '?';
    return s;
  }
  std::size_t getSize() const { return value.size(); }
  bool isEmpty() const { return value.empty(); }
  void erase(std::size_t p, std::size_t n = 1) { value.erase(p, n); }
  void insert(std::size_t p, const String &s) { value.insert(p, s.value); }
  Uint32 operator[](std::size_t i) const { return value[i]; }
  auto begin() const { return value.begin(); }
  auto end() const { return value.end(); }
};
struct FloatRect {
  float left{}, top{}, width{}, height{};
  bool contains(float x, float y) const {
    return x >= left && y >= top && x < left + width && y < top + height;
  }
};
struct IntRect {
  int left{}, top{}, width{}, height{};
  IntRect() = default;
  IntRect(int a, int b, int c, int d) : left(a), top(b), width(c), height(d) {}
};
class Image {
  Vector2u size;
  std::vector<Uint8> pixels;

public:
  bool loadFromFile(const std::string &);
  void create(unsigned w, unsigned h, Color c = Color());
  Vector2u getSize() const { return size; }
  const Uint8 *getPixelsPtr() const { return pixels.data(); }
  void flipVertically();
  void flipHorizontally();
  bool saveToFile(const std::string &) const;
};
class Window;
class Texture {
  struct Data;
  std::shared_ptr<Data> data;
  Vector2u size;
  bool smooth = false, repeated = false;
  unsigned maxSize = 256;

public:
  void setMaximumSize(unsigned n) { maxSize = n; }
  bool loadFromFile(const std::string &);
  bool loadFromImage(const Image &);
  bool create(unsigned, unsigned);
  void update(const Window &);
  Image copyToImage() const;
  Vector2u getSize() const { return size; }
  void setSmooth(bool b) { smooth = b; }
  void setRepeated(bool b) { repeated = b; }
  static void bind(const Texture *);
  GLuint id() const;
};
struct RenderStates {
  RenderStates() = default;
  RenderStates(int) {}
  const Texture *texture = nullptr;
  static const RenderStates Default;
};
constexpr int BlendAlpha = 0;
class Drawable {
public:
  virtual ~Drawable() = default;
  virtual void render(const RenderStates &) const = 0;
};
class Transformable {
protected:
  Vector2f position, scale{1, 1}, origin;

public:
  void setPosition(float x, float y) { position = {x, y}; }
  void setPosition(Vector2f p) { position = p; }
  Vector2f getPosition() const { return position; }
  void setScale(float x, float y) { scale = {x, y}; }
  void setScale(Vector2f p) { scale = p; }
  Vector2f getScale() const { return scale; }
  void setOrigin(float x, float y) { origin = {x, y}; }
};
class Sprite : public Drawable, public Transformable {
  const Texture *texture = nullptr;
  IntRect rect;
  Color color = Color::White;

public:
  Sprite() = default;
  explicit Sprite(const Texture &t) { setTexture(t); }
  void setTexture(const Texture &t, bool reset = false) {
    texture = &t;
    if (reset || !rect.width)
      rect = {0, 0, (int)t.getSize().x, (int)t.getSize().y};
  }
  const Texture *getTexture() const { return texture; }
  void setTextureRect(IntRect r) { rect = r; }
  const IntRect &getTextureRect() const { return rect; }
  void setColor(Color c) { color = c; }
  FloatRect getLocalBounds() const {
    return {0, 0, (float)rect.width, (float)rect.height};
  }
  FloatRect getGlobalBounds() const {
    return {position.x, position.y, rect.width * scale.x,
            rect.height * scale.y};
  }
  void render(const RenderStates &) const override;
};
class RectangleShape : public Drawable, public Transformable {
  Vector2f size;
  Color fill = Color::White, outline = Color::White;
  float thickness = 0;

public:
  RectangleShape(Vector2f s = {}) : size(s) {}
  void setSize(Vector2f s) { size = s; }
  Vector2f getSize() const { return size; }
  void setFillColor(Color c) { fill = c; }
  void setOutlineColor(Color c) { outline = c; }
  void setOutlineThickness(float t) { thickness = t; }
  void render(const RenderStates &) const override;
};
class Font {
public:
  struct Impl;
  std::shared_ptr<Impl> impl;
  bool loadFromFile(const std::string &);
};
class Text : public Drawable, public Transformable {
  String value;
  const Font *font = nullptr;
  unsigned size = 30;
  Color color = Color::White;

public:
  Text() = default;
  Text(const String &s, const Font &f, unsigned n = 30)
      : value(s), font(&f), size(n) {}
  void setString(const String &s) { value = s; }
  const String &getString() const { return value; }
  void setFont(const Font &f) { font = &f; }
  void setCharacterSize(unsigned n) { size = n; }
  unsigned getCharacterSize() const { return size; }
  void setFillColor(Color c) { color = c; }
  void setOutlineColor(Color) {}
  FloatRect getLocalBounds() const;
  FloatRect getGlobalBounds() const {
    auto r = getLocalBounds();
    r.left += position.x;
    r.top += position.y;
    return r;
  }
  Vector2f findCharacterPos(std::size_t i) const;
  void render(const RenderStates &) const override;
};
struct Vertex {
  Vector2f position;
  Color color = Color::White;
  Vector2f texCoords;
  Vertex() = default;
  Vertex(Vector2f p, Color c, Vector2f uv = {})
      : position(p), color(c), texCoords(uv) {}
};
constexpr int Quads = 0;
class VertexArray : public Drawable {
  std::vector<Vertex> vertices;

public:
  VertexArray(int, std::size_t n) : vertices(n) {}
  Vertex &operator[](std::size_t i) { return vertices[i]; }
  void render(const RenderStates &) const override;
};
struct Keyboard {
  enum Key {
    Unknown = -1,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Num0,
    Num1,
    Num2,
    Num3,
    Num4,
    Num5,
    Num6,
    Num7,
    Num8,
    Num9,
    Escape,
    LControl,
    LShift,
    LAlt,
    RControl,
    RShift,
    RAlt,
    Space,
    Return,
    Enter = Return,
    BackSpace,
    Tab,
    PageUp,
    PageDown,
    End,
    Home,
    Insert,
    Delete,
    Add,
    Dash,
    Equal,
    Left,
    Right,
    Up,
    Down,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    KeyCount
  };
  static bool isKeyPressed(Key);
};
struct Mouse {
  static Vector2i getPosition() { return {0, 0}; }
};
struct Joystick {
  enum Axis { X, Y, Z, R, U, V };
  static constexpr unsigned Count = 1;
  static bool isConnected(unsigned) { return false; }
  static unsigned getButtonCount(unsigned) { return 0; }
  static bool hasAxis(unsigned, Axis) { return false; }
};
struct Event {
  enum EventType {
    Closed,
    Resized,
    KeyPressed,
    KeyReleased,
    TextEntered,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseMoved,
    JoystickMoved,
    JoystickButtonPressed,
    JoystickButtonReleased
  } type;
  struct {
    Keyboard::Key code;
  } key;
  struct {
    unsigned unicode;
  } text;
  struct {
    int button, x, y;
  } mouseButton;
  struct {
    int x, y;
  } mouseMove;
  struct {
    Joystick::Axis axis;
    float position;
  } joystickMove;
  struct {
    unsigned button;
  } joystickButton;
  struct {
    unsigned width, height;
  } size;
};
struct VideoMode {
  unsigned width, height, bitsPerPixel;
  VideoMode(unsigned w, unsigned h, unsigned b = 32)
      : width(w), height(h), bitsPerPixel(b) {}
  static VideoMode getDesktopMode() { return {854, 480, 16}; }
};
struct ContextSettings {
  ContextSettings(int, int, int, int, int) {}
};
struct Style {
  static constexpr unsigned Close = 1, Titlebar = 2, Fullscreen = 4;
};
struct Context {
  static void *getFunction(const char *) { return nullptr; }
};
class Window {
public:
  Vector2u getSize() const { return {854, 480}; }
};
class RenderWindow : public Window {
public:
  void create(VideoMode, const char *, unsigned, ContextSettings);
  void setFramerateLimit(unsigned) {}
  void setKeyRepeatEnabled(bool) {}
  void setMouseCursorVisible(bool) {}
  void close();
  void display();
  void clear(Color);
  void draw(const Drawable &, const RenderStates & = RenderStates::Default);
  void pushGLStates();
  void popGLStates();
  bool pollEvent(Event &);
};
class SoundBuffer {
public:
  struct Data;
  std::shared_ptr<Data> data;
  bool loadFromFile(const std::string &);
};
class Sound {
  const SoundBuffer *buffer = nullptr;
  int channel = -1;
  bool loop = false;
  float volume = 100;

public:
  ~Sound() { stop(); }
  enum Status { Stopped, Paused, Playing };
  void setBuffer(const SoundBuffer &b) { buffer = &b; }
  void setVolume(float);
  void setLoop(bool b) { loop = b; }
  bool getLoop() const { return loop; }
  Status getStatus() const;
  void play();
  void stop();
};
class Music {
  struct Data;
  std::shared_ptr<Data> data;
  bool loop = false;
  float volume = 100;

public:
  bool openFromFile(const std::string &);
  void setVolume(float);
  void setLoop(bool b) { loop = b; }
  bool getLoop() const { return loop; }
  void play();
  void stop();
};
} // namespace sf
