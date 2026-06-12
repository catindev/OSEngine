/*
 * glad — OpenGL loader (Core profile, OpenGL 3.3)
 * Generated from https://glad.dav1d.de (public domain / MIT)
 * This is a minimal subset header. For full generation, use https://glad.dav1d.de
 *
 * SPDX-License-Identifier: (WTFPL OR CC0-1.0) AND Apache-2.0
 */
#ifndef GLAD_H
#define GLAD_H

#ifdef __gl_h_
#error OpenGL header already included. Include glad/glad.h first.
#endif
#define __gl_h_

#if defined(_WIN32) && !defined(APIENTRY) && !defined(__CYGWIN__) && !defined(__SCITECH_SNAP__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#endif

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif
#ifndef GLAPI
#define GLAPI extern
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void*        (*GLADloadproc)(const char* name);
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int          GLint;
typedef int          GLsizei;
typedef unsigned char GLboolean;
typedef signed char  GLbyte;
typedef short        GLshort;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned long  GLulong;
typedef float        GLfloat;
typedef float        GLclampf;
typedef double       GLdouble;
typedef double       GLclampd;
typedef void         GLvoid;
typedef int64_t      GLint64;
typedef uint64_t     GLuint64;
typedef ptrdiff_t    GLsizeiptr;
typedef ptrdiff_t    GLintptr;
typedef char         GLchar;

// ─── OpenGL constants ─────────────────────────────────────────────────────────

#define GL_FALSE                          0
#define GL_TRUE                           1
#define GL_NO_ERROR                       0
#define GL_ZERO                           0
#define GL_ONE                            1
#define GL_NONE                           0

#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_STENCIL_BUFFER_BIT             0x00000400
#define GL_COLOR_BUFFER_BIT               0x00004000

#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_LINE_LOOP                      0x0002
#define GL_LINE_STRIP                     0x0003
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_TRIANGLE_FAN                   0x0006

#define GL_NEVER                          0x0200
#define GL_LESS                           0x0201
#define GL_EQUAL                          0x0202
#define GL_LEQUAL                         0x0203
#define GL_GREATER                        0x0204
#define GL_NOTEQUAL                       0x0205
#define GL_GEQUAL                         0x0206
#define GL_ALWAYS                         0x0207

#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_ALPHA                      0x0304
#define GL_ONE_MINUS_DST_ALPHA            0x0305
#define GL_DST_COLOR                      0x0306
#define GL_ONE_MINUS_DST_COLOR            0x0307

#define GL_FRONT                          0x0404
#define GL_BACK                           0x0405
#define GL_FRONT_AND_BACK                 0x0408

#define GL_INVALID_ENUM                   0x0500
#define GL_INVALID_VALUE                  0x0501
#define GL_INVALID_OPERATION              0x0502
#define GL_OUT_OF_MEMORY                  0x0505

#define GL_CW                             0x0900
#define GL_CCW                            0x0901

#define GL_CULL_FACE                      0x0B44
#define GL_LIGHTING                       0x0B50
#define GL_DEPTH_TEST                     0x0B71
#define GL_BLEND                          0x0BE2
#define GL_TEXTURE_2D                     0x0DE1

#define GL_BYTE                           0x1400
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_SHORT                          0x1402
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405
#define GL_FLOAT                          0x1406

#define GL_RGBA                           0x1908
#define GL_RGB                            0x1907
#define GL_LUMINANCE                      0x1909
#define GL_RGBA8                          0x8058
#define GL_RGB8                           0x8051

#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_NEAREST_MIPMAP_NEAREST         0x2700
#define GL_LINEAR_MIPMAP_NEAREST          0x2701
#define GL_NEAREST_MIPMAP_LINEAR          0x2702
#define GL_LINEAR_MIPMAP_LINEAR           0x2703

#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_REPEAT                         0x2901
#define GL_CLAMP_TO_EDGE                  0x812F

#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_STATIC_DRAW                    0x88B4
#define GL_DYNAMIC_DRAW                   0x88E8

#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84

#define GL_VENDOR                         0x1F00
#define GL_RENDERER                       0x1F01
#define GL_VERSION                        0x1F02
#define GL_EXTENSIONS                     0x1F03

#define GL_MAX_TEXTURE_SIZE               0x0D33
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE1                       0x84C1

// ─── Function pointers ────────────────────────────────────────────────────────

GLAPI int gladLoadGLLoader(GLADloadproc);

GLAPI void   (APIENTRYP glViewport)(GLint x, GLint y, GLsizei w, GLsizei h);
GLAPI void   (APIENTRYP glClear)(GLbitfield mask);
GLAPI void   (APIENTRYP glClearColor)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
GLAPI void   (APIENTRYP glEnable)(GLenum cap);
GLAPI void   (APIENTRYP glDisable)(GLenum cap);
GLAPI void   (APIENTRYP glDepthMask)(GLboolean flag);
GLAPI void   (APIENTRYP glCullFace)(GLenum mode);
GLAPI void   (APIENTRYP glFrontFace)(GLenum mode);
GLAPI void   (APIENTRYP glBlendFunc)(GLenum sfactor, GLenum dfactor);
GLAPI void   (APIENTRYP glGetIntegerv)(GLenum pname, GLint* data);
GLAPI const GLubyte* (APIENTRYP glGetString)(GLenum name);
GLAPI GLenum (APIENTRYP glGetError)(void);

// Textures
GLAPI void   (APIENTRYP glGenTextures)(GLsizei n, GLuint* textures);
GLAPI void   (APIENTRYP glDeleteTextures)(GLsizei n, const GLuint* textures);
GLAPI void   (APIENTRYP glBindTexture)(GLenum target, GLuint texture);
GLAPI void   (APIENTRYP glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
GLAPI void   (APIENTRYP glTexParameteri)(GLenum target, GLenum pname, GLint param);
GLAPI void   (APIENTRYP glGenerateMipmap)(GLenum target);
GLAPI void   (APIENTRYP glActiveTexture)(GLenum texture);

// Buffers / VAO
GLAPI void   (APIENTRYP glGenBuffers)(GLsizei n, GLuint* buffers);
GLAPI void   (APIENTRYP glDeleteBuffers)(GLsizei n, const GLuint* buffers);
GLAPI void   (APIENTRYP glBindBuffer)(GLenum target, GLuint buffer);
GLAPI void   (APIENTRYP glBufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
GLAPI void   (APIENTRYP glGenVertexArrays)(GLsizei n, GLuint* arrays);
GLAPI void   (APIENTRYP glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
GLAPI void   (APIENTRYP glBindVertexArray)(GLuint array);
GLAPI void   (APIENTRYP glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
GLAPI void   (APIENTRYP glEnableVertexAttribArray)(GLuint index);
GLAPI void   (APIENTRYP glDrawArrays)(GLenum mode, GLint first, GLsizei count);
GLAPI void   (APIENTRYP glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void* indices);

// Shaders
GLAPI GLuint (APIENTRYP glCreateShader)(GLenum type);
GLAPI void   (APIENTRYP glDeleteShader)(GLuint shader);
GLAPI void   (APIENTRYP glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
GLAPI void   (APIENTRYP glCompileShader)(GLuint shader);
GLAPI void   (APIENTRYP glGetShaderiv)(GLuint, GLenum, GLint*);
GLAPI void   (APIENTRYP glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
GLAPI GLuint (APIENTRYP glCreateProgram)(void);
GLAPI void   (APIENTRYP glDeleteProgram)(GLuint program);
GLAPI void   (APIENTRYP glAttachShader)(GLuint program, GLuint shader);
GLAPI void   (APIENTRYP glLinkProgram)(GLuint program);
GLAPI void   (APIENTRYP glGetProgramiv)(GLuint, GLenum, GLint*);
GLAPI void   (APIENTRYP glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
GLAPI void   (APIENTRYP glUseProgram)(GLuint program);
GLAPI GLint  (APIENTRYP glGetUniformLocation)(GLuint, const GLchar*);
GLAPI void   (APIENTRYP glUniform1i)(GLint, GLint);
GLAPI void   (APIENTRYP glUniform1f)(GLint, GLfloat);
GLAPI void   (APIENTRYP glUniform2f)(GLint, GLfloat, GLfloat);
GLAPI void   (APIENTRYP glUniform3f)(GLint, GLfloat, GLfloat, GLfloat);
GLAPI void   (APIENTRYP glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
GLAPI void   (APIENTRYP glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);

#ifdef __cplusplus
}
#endif

#endif // GLAD_H
