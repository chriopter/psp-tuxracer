// PSP port, 2026-10-01: the buffer-object calls PSPGL implements, declared
// here because its glext.h only does so behind GL_GLEXT_PROTOTYPES.
// Geometry in a buffer object in the GE's own layout is drawn by index with
// no copy; GL_DYNAMIC_DRAW keeps it in system memory, away from the textures.
#ifndef PSP_BUFFERS_H
#define PSP_BUFFERS_H

#include "bh.h"

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif
extern "C" {
void glGenBuffers(GLsizei, GLuint*);
void glBindBuffer(GLenum, GLuint);
void glBufferData(GLenum, long, const GLvoid*, GLenum);
void glDeleteBuffers(GLsizei, const GLuint*);
void* glMapBuffer(GLenum, GLenum);
GLboolean glUnmapBuffer(GLenum);
}
#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
#endif

// The course's vertex array, held in a buffer object from the start, so the
// GE draws from the very memory the game fills and reads: one copy of the
// course, not two. Null from Alloc where no buffer can be had; the caller
// then uses memory of its own and the terrain is drawn the copying way.
GLubyte* PspTerrainArrayAlloc(long bytes);
void PspTerrainArrayFilled(GLubyte* array, long bytes);
bool PspTerrainArrayFree(GLubyte* array);   // true if the array was the buffer's

#endif
