// SPDX-License-Identifier: GPL-2.0-or-later
// PSP platform layer for the official Extreme Tux Racer PC sources.
#include "course.h"
#include <cerrno>
#include "savedata.hpp"
#include "game_ctrl.h"
#include "game_type_select.h"
#include "intro.h"
#include "ogl.h"
#include "paused.h"
#include "physics.h"
#include "race_select.h"
#include "racing.h"
#include "regist.h"
#include "states.h"
#include "winsys.h"
#include <GLES/egl.h>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <SFML/psp.hpp>
#include <cstdio>
#include <cstring>
#include <deque>
#include <malloc.h>
#include <new>
#include <pspctrl.h>
#include <psppower.h>
#include <sys/stat.h>
#include <unistd.h>
#undef main
PSP_MODULE_INFO("Extreme Tux Racer", 0, 0, 84);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
// Negative heap size means maximum; only the threshold reserves utility memory.
PSP_HEAP_SIZE_KB(-1);
PSP_HEAP_THRESHOLD_SIZE_KB(4096);
static EGLDisplay egl_display;
static EGLSurface surface;
static volatile bool running = true;
static bool gui = false;
bool PspIsRunning() { return running; }
static unsigned benchmark = 0, benchframe = 0, benchstateframe = 0;
static std::string benchcourse = "frozen_river";
static State *benchstate = nullptr;
static bool benchmark_recording = false;
static std::vector<unsigned> benchmark_times, benchmark_steering;
static unsigned benchmark_music_frames = 0;
static std::vector<unsigned> benchmark_work;
static unsigned heap_peak = 0, free_user_min = ~0u;
void PspTraceResource(const char *phase, const char *path) {
  const auto memory = mallinfo();
  fprintf(stderr,
          "RESOURCE %s: %s heap_used=%u heap_free=%u free_user=%u largest_free=%u stack_check=%d\n",
          phase, path, (unsigned)memory.uordblks, (unsigned)memory.fordblks,
          sceKernelTotalFreeMemSize(), sceKernelMaxFreeMemSize(),
          sceKernelCheckThreadStack());
}
static void finish_benchmark() {
  if (!benchmark_recording || benchmark_times.empty())
    return;
  benchmark_recording = false;
  benchmark = 0;
  auto statistics = [](std::vector<unsigned> &values, FILE *f) {
    uint64_t sum = 0;
    unsigned slow = 0;
    for (unsigned value : values) {
      sum += value;
      if (value > 35000)
        ++slow;
    }
    std::sort(values.begin(), values.end());
    fprintf(f,
            "{\"frames\":%u,\"fps\":%.3f,\"median_us\":%u,\"p95_us\":%u,\"max_"
            "us\":%u,\"over35ms\":%u}",
            (unsigned)values.size(),
            sum ? values.size() * 1000000.f / sum : 0.f,
            values.empty() ? 0 : values[values.size() / 2],
            values.empty() ? 0 : values[(values.size() - 1) * 95 / 100],
            values.empty() ? 0 : values.back(), slow);
  };
  FILE *f = fopen("config/benchmark-result.json", "w");
  if (!f)
    return;
  fprintf(
      f,
      "{\"game\":\"Extreme Tux Racer "
      "0.8.4\",\"course\":\"%s\",\"cpu_mhz\":%d,\"music_frames\":%u,\"all\":",
      benchcourse.c_str(), scePowerGetCpuClockFrequencyInt(),
      benchmark_music_frames);
  statistics(benchmark_times, f);
  fprintf(f, ",\"steering\":");
  statistics(benchmark_steering, f);
  std::sort(benchmark_work.begin(), benchmark_work.end());
  fprintf(f, ",\"work\":{\"median_us\":%u,\"p95_us\":%u,\"max_us\":%u},\"heap_peak_bytes\":%u,\"min_free_user_bytes\":%u}\n",
          benchmark_work.empty() ? 0 : benchmark_work[benchmark_work.size()/2],
          benchmark_work.empty() ? 0 : benchmark_work[(benchmark_work.size()-1)*95/100],
          benchmark_work.empty() ? 0 : benchmark_work.back(), heap_peak, free_user_min);
  fclose(f);
}

extern void EnterPractice();
static std::deque<sf::Event> events;
static bool keys[sf::Keyboard::KeyCount] = {};
static bool inputSuppressed = false;
void PspResetInput() { events.clear(); std::memset(keys, 0, sizeof(keys)); inputSuppressed = true; }
static int exit_cb(int, int, void *) {
  running = false;
  return 0;
}
static int callbacks(SceSize, void *) {
  sceKernelRegisterExitCallback(
      sceKernelCreateCallback("ETR exit", exit_cb, nullptr));
  sceKernelSleepThreadCB();
  return 0;
}
extern int etr_main(int, char **);
int main(int argc, char **argv) {
  freopen("etr.log", "w", stdout);
  freopen("etr-errors.log", "w", stderr);
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  char working_directory[1024]{};
  getcwd(working_directory, sizeof(working_directory));
  fprintf(stderr, "ETR startup: firmware=0x%08x cwd=%s argv0=%s\n",
          sceKernelDevkitVersion(), working_directory,
          argc > 0 && argv && argv[0] ? argv[0] : "(none)");
  mkdir("config", 0777);
  scePowerSetClockFrequency(333, 333, 166);
  int t = sceKernelCreateThread("ETR callbacks", callbacks, 0x11, 0x1000, 0,
                                nullptr);
  if (t >= 0)
    sceKernelStartThread(t, 0, nullptr);
  sceCtrlSetSamplingCycle(0);
  sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)
    fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
  if (TTF_Init() < 0)
    fprintf(stderr, "Font initialization failed: %s\n", TTF_GetError());
  if (Mix_OpenAudio(22050, AUDIO_S16SYS, 2, 1024) < 0)
    fprintf(stderr, "Audio initialization failed: %s\n", Mix_GetError());
  if (Mix_AllocateChannels(12) != 12)
    fprintf(stderr, "Audio channel allocation failed: %s\n", Mix_GetError());
  auto memory = mallinfo();
  fprintf(stderr, "ETR memory: heap_total=%u heap_used=%u free_user=%u largest_free=%u\n",
          (unsigned)memory.arena, (unsigned)memory.uordblks,
          sceKernelTotalFreeMemSize(), sceKernelMaxFreeMemSize());
  FILE *bf = fopen("config/benchmark", "r");
  if (bf) {
    char course[128] = {};
    int parsed = fscanf(bf, "%u %127s", &benchmark, course);
    fclose(bf);
    if (parsed >= 1 && benchmark > 0) {
      if (course[0]) benchcourse = course;
      benchmark = std::max(60u, std::min(7200u, benchmark));
      benchmark_recording = true;
      benchmark_times.reserve(7200);
      benchmark_work.reserve(7200);
      benchmark_steering.reserve(7200);
      remove("config/timing.log");
      remove("config/benchmark-result.json");
    } else {
      benchmark = 0;
      fprintf(stderr, "Ignoring empty or invalid config/benchmark\n");
    }
  }
  PspSave::SetBenchmark(benchmark != 0);
  printf("ETR PSP: CPU %d MHz\n", scePowerGetCpuClockFrequencyInt());
  int result = etr_main(argc, argv);
  Mix_CloseAudio();
  SDL_Quit();
  sceKernelExitGame();
  return result;
}
namespace sf {
const Color Color::White{255, 255, 255}, Color::Black{0, 0, 0},
    Color::Red{255, 0, 0}, Color::Blue{0, 0, 255}, Color::Yellow{255, 255, 0},
    Color::Transparent{0, 0, 0, 0};
const RenderStates RenderStates::Default{};
static const Sound *sound_owners[12] = {};
struct V {
  float u, v;
  Color c;
  float x, y, z;
};
static void drawVertices(const std::vector<V> &v, const Texture *t) {
  if (v.empty())
    return;
  if (t) {
    glEnable(GL_TEXTURE_2D);
    Texture::bind(t);
  } else
    glDisable(GL_TEXTURE_2D);
  glDisableClientState(GL_NORMAL_ARRAY);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glTexCoordPointer(2, GL_FLOAT, sizeof(V), &v[0].u);
  glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(V), &v[0].c);
  glVertexPointer(3, GL_FLOAT, sizeof(V), &v[0].x);
  glDrawArrays(GL_TRIANGLES, 0, v.size());
  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_COLOR_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}
static void quad(std::vector<V> &v, float x, float y, float w, float h, float u,
                 float t, float du, float dt, Color c) {
  V a{u, t, c, x, y, 0}, b{u + du, t, c, x + w, y, 0},
      d{u, t + dt, c, x, y + h, 0}, e{u + du, t + dt, c, x + w, y + h, 0};
  v.insert(v.end(), {a, b, d, b, e, d});
}
void Image::create(unsigned w, unsigned h, Color c) {
  size = {w, h};
  pixels.resize(w * h * 4);
  for (unsigned i = 0; i < w * h; i++)
    memcpy(&pixels[i * 4], &c, 4);
}
bool Image::loadFromFile(const std::string &p) {
  PspTraceResource("decode begin", p.c_str());
  SDL_Surface *s = IMG_Load(p.c_str());
  if (!s) {
    fprintf(stderr, "image %s: %s\n", p.c_str(), IMG_GetError());
    return false;
  }
  fprintf(stderr, "IMAGE decoded: %s width=%d height=%d pitch=%u bpp=%u\n",
          p.c_str(), s->w, s->h, (unsigned)s->pitch,
          (unsigned)s->format->BytesPerPixel);
  PspTraceResource("convert begin", p.c_str());
  create(s->w, s->h);
  SDL_LockSurface(s);
  for (unsigned y = 0; y < size.y; y++)
    for (unsigned x = 0; x < size.x; x++) {
      Uint32 c = 0;
      memcpy(&c,
             (char *)s->pixels + y * s->pitch + x * s->format->BytesPerPixel,
             s->format->BytesPerPixel);
      SDL_GetRGBA(c, s->format, &pixels[(y * size.x + x) * 4],
                  &pixels[(y * size.x + x) * 4 + 1],
                  &pixels[(y * size.x + x) * 4 + 2],
                  &pixels[(y * size.x + x) * 4 + 3]);
    }
  SDL_UnlockSurface(s);
  SDL_FreeSurface(s);
  PspTraceResource("image ready", p.c_str());
  return true;
}
void Image::flipVertically() {
  for (unsigned y = 0; y < size.y / 2; y++)
    for (unsigned x = 0; x < size.x * 4; x++)
      std::swap(pixels[y * size.x * 4 + x],
                pixels[(size.y - 1 - y) * size.x * 4 + x]);
}
void Image::flipHorizontally() {
  for (unsigned y = 0; y < size.y; y++)
    for (unsigned x = 0; x < size.x / 2; x++)
      for (unsigned c = 0; c < 4; c++)
        std::swap(pixels[(y * size.x + x) * 4 + c],
                  pixels[(y * size.x + size.x - 1 - x) * 4 + c]);
}
bool Image::saveToFile(const std::string &) const {
  fprintf(stderr, "Use PPSSPP screenshot capture.\n");
  return false;
}
struct Texture::Data {
  GLuint id = 0;
  Data() { glGenTextures(1, &id); }
  ~Data() { glDeleteTextures(1, &id); }
};
GLuint Texture::id() const { return data ? data->id : 0; }
bool Texture::loadFromFile(const std::string &p) {
  Image i;
  if (p.find("preview.png") != std::string::npos)
    maxSize = 128;
  printf("texture %s\n", p.c_str());
  if (!i.loadFromFile(p))
    return false;
  PspTraceResource("upload begin", p.c_str());
  const bool loaded = loadFromImage(i);
  PspTraceResource(loaded ? "upload returned" : "upload failed", p.c_str());
  return loaded;
}
static unsigned pot(unsigned n) {
  unsigned p = 8;
  while (p < n && p < 512)
    p *= 2;
  return p;
}
bool Texture::loadFromImage(const Image &i) {
  size = i.getSize();
  if (!size.x || !size.y)
    return false;
  unsigned w = std::min(maxSize, pot(size.x)),
           h = std::min(maxSize, pot(size.y));
  std::vector<Uint8> p(w * h * 4);
  const Uint8 *src = i.getPixelsPtr();
  for (unsigned y = 0; y < h; y++)
    for (unsigned x = 0; x < w; x++)
      memcpy(&p[(y * w + x) * 4],
             &src[((y * size.y / h) * size.x + x * size.x / w) * 4], 4);
  data = std::make_shared<Data>();
  bind(this);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  bool opaque = true;
  for (unsigned k = 0; k < w * h; k++)
    if (p[k * 4 + 3] != 255) {
      opaque = false;
      break;
    }
  std::vector<uint16_t> packed(w * h);
  for (unsigned k = 0; k < w * h; k++)
    packed[k] =
        opaque ? ((p[k * 4] >> 3) | ((p[k * 4 + 1] >> 2) << 5) |
                  ((p[k * 4 + 2] >> 3) << 11))
               : ((p[k * 4] >> 4) | ((p[k * 4 + 1] >> 4) << 4) |
                  ((p[k * 4 + 2] >> 4) << 8) | ((p[k * 4 + 3] >> 4) << 12));
  glTexImage2D(GL_TEXTURE_2D, 0, opaque ? GL_RGB : GL_RGBA, w, h, 0,
               opaque ? GL_RGB : GL_RGBA,
               opaque ? GL_UNSIGNED_SHORT_5_6_5_REV
                      : GL_UNSIGNED_SHORT_4_4_4_4_REV,
               packed.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  smooth ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  smooth ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  repeated ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                  repeated ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  return true;
}
bool Texture::create(unsigned w, unsigned h) {
  Image i;
  i.create(w, h);
  return loadFromImage(i);
}
void Texture::update(const Window &) {
  fprintf(stderr, "PSP framebuffer capture is provided by PPSSPP.\n");
}
Image Texture::copyToImage() const { return {}; }
void Texture::bind(const Texture *t) {
  glBindTexture(GL_TEXTURE_2D, t ? t->id() : 0);
}
void Sprite::render(const RenderStates &) const {
  if (!texture)
    return;
  auto s = texture->getSize();
  std::vector<V> v;
  quad(v, position.x - origin.x * scale.x, position.y - origin.y * scale.y,
       rect.width * scale.x, rect.height * scale.y, (float)rect.left / s.x,
       (float)rect.top / s.y, (float)rect.width / s.x, (float)rect.height / s.y,
       color);
  drawVertices(v, texture);
}
void RectangleShape::render(const RenderStates &) const {
  std::vector<V> v;
  float x = position.x - origin.x, y = position.y - origin.y,
        w = size.x * scale.x, h = size.y * scale.y;
  if (thickness) {
    quad(v, x - thickness, y - thickness, w + 2 * thickness, h + 2 * thickness,
         0, 0, 0, 0, outline);
  }
  quad(v, x, y, w, h, 0, 0, 0, 0, fill);
  drawVertices(v, nullptr);
}
struct Font::Impl {
  Texture atlas;
  struct Glyph {
    float advance = 12;
    int x = 0, y = 0, w = 0, h = 0, top = 0, left = 0;
  } glyph[256];
  int ascent = 24;
};
bool Font::loadFromFile(const std::string &p) {
  TTF_Font *f = TTF_OpenFont(p.c_str(), 24);
  if (!f) {
    fprintf(stderr, "font %s: %s\n", p.c_str(), TTF_GetError());
    return false;
  }
  impl = std::make_shared<Impl>();
  impl->ascent = TTF_FontAscent(f);
  // Transparent white gutters prevent neighboring glyphs and dark fringes
  // from leaking into text under bilinear filtering.
  std::vector<Uint8> pixels(512 * 512 * 4, 255);
  for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 0;
  int atlasX = 1, atlasY = 1, rowHeight = 0;
  for (unsigned c = 32; c < 256; c++) {
    int minx, maxx, miny, maxy, advance;
    auto &g = impl->glyph[c];
    if (TTF_GlyphMetrics(f, c, &minx, &maxx, &miny, &maxy, &advance) < 0)
      continue;
    g.advance = advance;
    g.left = minx;
    g.top = impl->ascent - maxy;
    SDL_Color white{255, 255, 255, 0};
    SDL_Surface *s = TTF_RenderGlyph_Blended(f, c, white);
    if (!s)
      continue;
    if (atlasX + s->w + 1 > 512) {
      atlasX = 1; atlasY += rowHeight + 2; rowHeight = 0;
    }
    if (s->w + 2 > 512 || atlasY + s->h + 1 > 512) {
      fprintf(stderr, "Font atlas overflow: %s, glyph %u\n", p.c_str(), c);
      SDL_FreeSurface(s); TTF_CloseFont(f); impl.reset();
      return false;
    }
    g.x = atlasX; g.y = atlasY;
    g.w = s->w; g.h = s->h;
    atlasX += s->w + 2;
    rowHeight = std::max(rowHeight, s->h);
    // SDL_ttf returns a cropped glyph bitmap, not a complete text line.
    // Retain minx and ascent-maxy so punctuation and descenders align.
    for (int y = 0; y < g.h; y++)
      for (int x = 0; x < g.w; x++) {
        Uint32 q;
        memcpy(&q, (char *)s->pixels + y * s->pitch + x * 4, 4);
        Uint8 r, b, gr, a;
        SDL_GetRGBA(q, s->format, &r, &gr, &b, &a);
        auto d = &pixels[((g.y + y) * 512 + g.x + x) * 4];
        d[0] = d[1] = d[2] = 255;
        d[3] = a;
      }
    SDL_FreeSurface(s);
  }
  TTF_CloseFont(f);
  Image image;
  image.create(512, 512);
  memcpy(const_cast<Uint8 *>(image.getPixelsPtr()), pixels.data(),
         pixels.size());
  impl->atlas.setMaximumSize(512);
  impl->atlas.setSmooth(true);
  return impl->atlas.loadFromImage(image);
}
FloatRect Text::getLocalBounds() const {
  if (!font || !font->impl)
    return {};
  float width = 0, line = 0, height = size;
  for (auto c : value) {
    if (c == '\r') continue;
    if (c == '\n') {
      width = std::max(width, line); line = 0; height += size * 1.25f;
    } else line += font->impl->glyph[c < 256 ? c : '?'].advance * size / 24.f;
  }
  return {0, size * 0.15f, std::max(width, line), height};
}
Vector2f Text::findCharacterPos(std::size_t i) const {
  float x = 0, y = 0;
  if (font && font->impl)
    for (size_t k = 0; k < std::min(i, value.getSize()); k++) {
      auto c = value[k];
      if (c == '\r') continue;
      if (c == '\n') { x = 0; y += size * 1.25f; }
      else x += font->impl->glyph[c < 256 ? c : '?'].advance * size / 24.f;
    }
  return {position.x + x, position.y + y};
}
void Text::render(const RenderStates &) const {
  if (!font || !font->impl)
    return;
  static std::vector<V> v;
  v.clear();
  v.reserve(value.getSize() * 6);
  float x = position.x - origin.x, y = position.y - origin.y, s = size / 24.f;
  for (auto c : value) {
    if (c == '\r') continue;
    if (c == '\n') { x = position.x - origin.x; y += size * 1.25f; continue; }
    auto &g = font->impl->glyph[c < 256 ? c : '?'];
    quad(v, x + g.left * s, y + g.top * s, g.w * s, g.h * s, g.x / 512.f,
         g.y / 512.f, g.w / 512.f, g.h / 512.f, color);
    x += g.advance * s;
  }
  drawVertices(v, &font->impl->atlas);
}
void VertexArray::render(const RenderStates &s) const {
  static std::vector<V> v;
  v.clear();
  auto z = s.texture ? s.texture->getSize() : Vector2u{1, 1};
  for (size_t i = 0; i + 3 < vertices.size(); i += 4)
    for (int j : {0, 1, 2, 0, 2, 3}) {
      auto &a = vertices[i + j];
      v.push_back({a.texCoords.x / z.x, a.texCoords.y / z.y, a.color,
                   a.position.x, a.position.y, 0});
    }
  drawVertices(v, s.texture);
}
void RenderWindow::create(VideoMode, const char *, unsigned, ContextSettings) {
  if (egl_display)
    return;
  EGLConfig config;
  EGLint n;
  const EGLint attrs[] = {EGL_RED_SIZE,  5, EGL_GREEN_SIZE, 6,
                          EGL_BLUE_SIZE, 5, EGL_DEPTH_SIZE, 16,
                          EGL_NONE};
  egl_display = eglGetDisplay(0);
  if (!eglInitialize(egl_display, nullptr, nullptr) ||
      !eglChooseConfig(egl_display, attrs, &config, 1, &n) || !n) {
    fprintf(stderr, "EGL init failed\n");
    sceKernelExitGame();
  }
  EGLContext ctx = eglCreateContext(egl_display, config, nullptr, nullptr);
  surface = eglCreateWindowSurface(egl_display, config, 0, nullptr);
  eglMakeCurrent(egl_display, surface, surface, ctx);
  eglSwapInterval(egl_display, 1);
  glViewport(0, 0, 480, 272);
  const GLfloat ambient[] = {0.2f, 0.2f, 0.2f, 1.f};
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
  printf("PSP graphics: %s\n", glGetString(GL_RENDERER));
}
void RenderWindow::close() { running = false; }
void RenderWindow::display() {
  ResetRenderMode();
  auto before_present = benchmark_recording ? sceKernelGetSystemTimeWide() : 0;
  eglSwapBuffers(egl_display, surface);
  // Never write profiling logs to the Memory Stick during normal play.
  if (!benchmark_recording) return;
  static uint64_t last = 0, total = 0;
  static unsigned frames = 0, worst = 0;
  static State *previous = nullptr;
  auto now = sceKernelGetSystemTimeWide();
  auto state = State::manager.CurrentState();
  if (state == &Racing && previous == state && last) {
    unsigned dt = now - last;
    if (benchmark_recording) {
      benchmark_times.push_back(dt);
      benchmark_work.push_back(before_present - last);
      if (benchmark_times.size() % 60 == 1) {
        heap_peak = std::max(heap_peak, (unsigned)mallinfo().uordblks);
        free_user_min = std::min(free_user_min, (unsigned)sceKernelTotalFreeMemSize());
      }
      if (keys[Keyboard::Left] || keys[Keyboard::Right])
        benchmark_steering.push_back(dt);
      if (Mix_PlayingMusic())
        ++benchmark_music_frames;
    }
    total += dt;
    worst = std::max(worst, dt);
    if (++frames == 300) {
      FILE *f = fopen("config/timing.log", "a");
      if (f) {
        fprintf(f,
                "ETR timing: cpu=%d frames=%u fps=%.3f worst_us=%u music=%d "
                "position=%.2f,%.2f,%.2f\n",
                scePowerGetCpuClockFrequencyInt(), frames,
                frames * 1000000.f / total, worst, Mix_PlayingMusic(),
                g_game.player->ctrl->cpos.x, g_game.player->ctrl->cpos.y,
                g_game.player->ctrl->cpos.z);
        fclose(f);
      }
      frames = 0;
      total = 0;
      worst = 0;
    }
  } else {
    if (previous == &Racing)
      finish_benchmark();
    frames = 0;
    total = 0;
    worst = 0;
  }
  previous = state;
  last = now;
}
void RenderWindow::clear(Color c) {
  glClearColor(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
  glClear(GL_COLOR_BUFFER_BIT);
}
void RenderWindow::pushGLStates() {
  if (gui)
    return;
  gui = true;
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, 854, 480, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glDisable(GL_CULL_FACE);
  glDisable(GL_FOG);
  glDisable(GL_ALPHA_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}
void RenderWindow::popGLStates() {
  if (!gui)
    return;
  gui = false;
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
}
void RenderWindow::draw(const Drawable &d, const RenderStates &s) {
  bool temporary = !gui;
  if (temporary)
    pushGLStates();
  d.render(s);
  if (temporary)
    popGLStates();
}
bool Keyboard::isKeyPressed(Key k) { return k >= 0 && k < KeyCount && keys[k]; }
bool RenderWindow::pollEvent(Event &e) {
  static bool sampled = false;
  if (!events.empty()) {
    e = events.front();
    events.pop_front();
    return true;
  }
  if (sampled) {
    sampled = false;
    return false;
  }
  sampled = true;
  if (benchmark) {
    State *state = State::manager.CurrentState();
    if (state != benchstate) {
      benchstate = state;
      benchstateframe = 0;
    }
    ++benchstateframe;
    if (benchstateframe == 15) {
      if (state == &Regist) {
        e.type = Event::KeyPressed;
        e.key.code = Keyboard::Return;
        return true;
      }
      if (state == &GameTypeSelect) {
        EnterPractice();
      }
      if (state == &RaceSelect) {
        g_game.course = Course.GetCourse("default", benchcourse);
        state->Exit();
        state->Enter();
        e.type = Event::KeyPressed;
        e.key.code = Keyboard::Return;
        return true;
      }
      if (state == &Intro) {
        e.type = Event::KeyPressed;
        e.key.code = Keyboard::Return;
        return true;
      }
    }
  }
  if (!running) {
    e.type = Event::Closed;
    return true;
  }
  SceCtrlData p;
  sceCtrlPeekBufferPositive(&p, 1);
  unsigned b = p.Buttons;
  if (inputSuppressed) {
    if (b || p.Lx < 75 || p.Lx > 180 || p.Ly < 75 || p.Ly > 180) return false;
    inputSuppressed = false;
  }
  if (benchmark && State::manager.CurrentState() == &Racing) {
    unsigned phase = benchframe++ % 240;
    b = PSP_CTRL_UP;
    if (phase >= 60 && phase < 90)
      b |= PSP_CTRL_LEFT;
    if (phase >= 150 && phase < 180)
      b |= PSP_CTRL_RIGHT;
    if (benchframe >= benchmark) {
      benchmark = 0;
      State::manager.RequestEnterState(Paused);
    }
  }
  if (p.Lx < 75)
    b |= PSP_CTRL_LEFT;
  if (p.Lx > 180)
    b |= PSP_CTRL_RIGHT;
  if (p.Ly < 75)
    b |= PSP_CTRL_UP;
  if (p.Ly > 180)
    b |= PSP_CTRL_DOWN;
  bool racing = State::manager.CurrentState() == &Racing;
  bool now[Keyboard::KeyCount] = {};
  const unsigned bits[] = {
      PSP_CTRL_LEFT,  PSP_CTRL_RIGHT,    PSP_CTRL_UP,      PSP_CTRL_DOWN,
      PSP_CTRL_CROSS, PSP_CTRL_CIRCLE,   PSP_CTRL_SQUARE,  PSP_CTRL_TRIANGLE,
      PSP_CTRL_START, PSP_CTRL_LTRIGGER, PSP_CTRL_RTRIGGER};
  Keyboard::Key map[] = {Keyboard::Left,
                         Keyboard::Right,
                         Keyboard::Up,
                         Keyboard::Down,
                         racing ? Keyboard::Space : Keyboard::Return,
                         Keyboard::Escape,
                         Keyboard::T,
                         Keyboard::R,
                         racing ? Keyboard::P : Keyboard::Return,
                         Keyboard::Down,
                         Keyboard::Up};
  // Latch contextual bindings until physical release. A held Start/Cross must
  // not become a second press when its first press changes the game state.
  static unsigned previous_buttons = 0;
  static Keyboard::Key held_map[11] = {};
  for (int i = 0; i < 11; i++) {
    if ((b & bits[i]) && !(previous_buttons & bits[i]))
      held_map[i] = map[i];
    if (b & bits[i])
      now[held_map[i]] = true;
  }
  previous_buttons = b;
  for (int i = 0; i < Keyboard::KeyCount; i++)
    if (keys[i] != now[i]) {
      Event q;
      q.type = now[i] ? Event::KeyPressed : Event::KeyReleased;
      q.key.code = (Keyboard::Key)i;
      events.push_back(q);
      keys[i] = now[i];
    }
  if (!events.empty()) {
    e = events.front();
    events.pop_front();
    return true;
  }
  sampled = false;
  return false;
}
struct SoundBuffer::Data {
  Mix_Chunk *chunk = nullptr;
  ~Data() {
    if (chunk)
      Mix_FreeChunk(chunk);
  }
};
bool SoundBuffer::loadFromFile(const std::string &p) {
  data = std::make_shared<Data>();
  data->chunk = Mix_LoadWAV(p.c_str());
  if (!data->chunk)
    fprintf(stderr, "sound %s: %s\n", p.c_str(), Mix_GetError());
  return data->chunk;
}
void Sound::setVolume(float v) {
  volume = v;
  if (channel >= 0 && sound_owners[channel] == this)
    Mix_Volume(channel, v * 128 / 100);
}
Sound::Status Sound::getStatus() const {
  return channel >= 0 && sound_owners[channel] == this && Mix_Playing(channel)
             ? Playing
             : Stopped;
}
void Sound::play() {
  if (buffer && buffer->data) {
    channel = Mix_PlayChannel(-1, buffer->data->chunk, loop ? -1 : 0);
    if (channel >= 0)
      sound_owners[channel] = this;
    setVolume(volume);
  }
}
void Sound::stop() {
  if (channel >= 0 && sound_owners[channel] == this) {
    Mix_HaltChannel(channel);
    sound_owners[channel] = nullptr;
  }
  channel = -1;
}
struct Music::Data {
  static Data *active;
  std::string filename;
  Mix_Music *music = nullptr;
  void close() {
    if (active == this) {
      Mix_HaltMusic();
      active = nullptr;
    }
    if (music) { Mix_FreeMusic(music); music = nullptr; }
  }
  ~Data() { close(); }
};
Music::Data *Music::Data::active = nullptr;
bool Music::openFromFile(const std::string &p) {
  // Validate without retaining a file handle for every track in music.lst.
  FILE *file = fopen(p.c_str(), "rb");
  if (!file) {
    fprintf(stderr, "music file %s: errno=%d (%s)\n", p.c_str(), errno, strerror(errno));
    return false;
  }
  fclose(file);
  data = std::make_shared<Data>();
  data->filename = p;
  return true;
}
void Music::setVolume(float v) {
  volume = v;
  if (data && Data::active == data.get()) Mix_VolumeMusic(v * 128 / 100);
}
void Music::play() {
  if (!data) return;
  // Close the old stream BEFORE opening the next one: real PSP storage has
  // tighter file-handle limits than PPSSPP's host filesystem.
  if (Data::active) Data::active->close();
  data->music = Mix_LoadMUS(data->filename.c_str());
  if (!data->music) {
    fprintf(stderr, "music decode %s: %s\n", data->filename.c_str(), Mix_GetError());
    return;
  }
  Mix_VolumeMusic(volume * 128 / 100);
  if (Mix_PlayMusic(data->music, loop ? -1 : 0) < 0) {
    fprintf(stderr, "music play %s: %s\n", data->filename.c_str(), Mix_GetError());
    data->close();
    return;
  }
  Data::active = data.get();
}
void Music::stop() { if (data) data->close(); }

} // namespace sf
