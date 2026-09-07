/* Desktop OpenGL compatibility for PSPGL. GPL-2.0-or-later. */
#include <GL/gl.h>
#include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>

static GLuint bound_texture;
static struct {
  GLint width, height;
} sizes[4096];
void __real_glBindTexture(GLenum, GLuint);
void __real_glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum,
                         GLenum, const GLvoid *);
void __wrap_glBindTexture(GLenum target, GLuint texture) {
  if (target == GL_TEXTURE_2D)
    bound_texture = texture;
  __real_glBindTexture(target, texture);
}
void __wrap_glTexImage2D(GLenum target, GLint level, GLint internal, GLsizei w,
                         GLsizei h, GLint border, GLenum format, GLenum type,
                         const GLvoid *pixels) {
  if (target == GL_TEXTURE_2D && !level && bound_texture < 4096) {
    sizes[bound_texture].width = w;
    sizes[bound_texture].height = h;
  }
  __real_glTexImage2D(target, level, internal, w, h, border, format, type,
                      pixels);
}
void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname,
                              GLint *out) {
  *out = 0;
  if (target != GL_TEXTURE_2D || bound_texture >= 4096)
    return;
  if (pname == GL_TEXTURE_WIDTH)
    *out = sizes[bound_texture].width >> level;
  if (pname == GL_TEXTURE_HEIGHT)
    *out = sizes[bound_texture].height >> level;
}
void glMultMatrixd(const GLdouble *m) {
  GLfloat f[16];
  for (int i = 0; i < 16; i++)
    f[i] = (GLfloat)m[i];
  glMultMatrixf(f);
}
void glColor3dv(const GLdouble *c) { glColor3f(c[0], c[1], c[2]); }
void glColor4dv(const GLdouble *c) { glColor4f(c[0], c[1], c[2], c[3]); }
void glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {
  glBegin(GL_TRIANGLE_STRIP);
  glVertex2f(x1, y1);
  glVertex2f(x2, y1);
  glVertex2f(x1, y2);
  glVertex2f(x2, y2);
  glEnd();
}
void glRecti(GLint x1, GLint y1, GLint x2, GLint y2) {
  glRectf(x1, y1, x2, y2);
}
void glGetBooleanv(GLenum pname, GLboolean *out) {
  if (pname == GL_DOUBLEBUFFER) {
    *out = GL_TRUE;
    return;
  }
  GLint value = 0;
  glGetIntegerv(pname, &value);
  *out = (value != 0);
}
/* GL quality hints do not change the rendering semantics. PSPGL has no hint
 * API. */
void glHint(GLenum target, GLenum mode) {
  (void)target;
  (void)mode;
}
const GLubyte *gluErrorString(GLenum error) {
  switch (error) {
  case GL_NO_ERROR:
    return (const GLubyte *)"no error";
  case GL_INVALID_ENUM:
    return (const GLubyte *)"invalid enum";
  case GL_INVALID_VALUE:
    return (const GLubyte *)"invalid value";
  case GL_INVALID_OPERATION:
    return (const GLubyte *)"invalid operation";
  case GL_OUT_OF_MEMORY:
    return (const GLubyte *)"out of memory";
  default:
    return (const GLubyte *)"OpenGL error";
  }
}

void __real_glEnable(GLenum);
void __real_glDisable(GLenum);
/* Terrain UVs are supplied explicitly; PSPGL has no desktop texgen switch.
   Unsupported toggles otherwise log synchronously to the Memory Stick. */
static int unsupported_cap(GLenum cap) {
  return cap == GL_COLOR_MATERIAL || cap == GL_TEXTURE_GEN_S ||
         cap == GL_TEXTURE_GEN_T;
}
void __wrap_glEnable(GLenum cap) {
  if (!unsupported_cap(cap))
    __real_glEnable(cap);
}
void __wrap_glDisable(GLenum cap) {
  if (!unsupported_cap(cap))
    __real_glDisable(cap);
}
