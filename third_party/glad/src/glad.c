/*
 * glad — OpenGL function loader implementation (Core profile 3.3)
 * Generated from https://glad.dav1d.de (public domain / MIT)
 * SPDX-License-Identifier: (WTFPL OR CC0-1.0) AND Apache-2.0
 */
#include "glad/glad.h"
#include <string.h>

// Function pointers
void   (APIENTRYP glViewport)(GLint x, GLint y, GLsizei w, GLsizei h);
void   (APIENTRYP glClear)(GLbitfield mask);
void   (APIENTRYP glClearColor)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void   (APIENTRYP glEnable)(GLenum cap);
void   (APIENTRYP glDisable)(GLenum cap);
void   (APIENTRYP glDepthMask)(GLboolean flag);
void   (APIENTRYP glCullFace)(GLenum mode);
void   (APIENTRYP glFrontFace)(GLenum mode);
void   (APIENTRYP glBlendFunc)(GLenum sfactor, GLenum dfactor);
void   (APIENTRYP glGetIntegerv)(GLenum pname, GLint* data);
const GLubyte* (APIENTRYP glGetString)(GLenum name);
GLenum (APIENTRYP glGetError)(void);

void   (APIENTRYP glGenTextures)(GLsizei n, GLuint* textures);
void   (APIENTRYP glDeleteTextures)(GLsizei n, const GLuint* textures);
void   (APIENTRYP glBindTexture)(GLenum target, GLuint texture);
void   (APIENTRYP glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
void   (APIENTRYP glTexParameteri)(GLenum target, GLenum pname, GLint param);
void   (APIENTRYP glGenerateMipmap)(GLenum target);
void   (APIENTRYP glActiveTexture)(GLenum texture);

void   (APIENTRYP glGenBuffers)(GLsizei n, GLuint* buffers);
void   (APIENTRYP glDeleteBuffers)(GLsizei n, const GLuint* buffers);
void   (APIENTRYP glBindBuffer)(GLenum target, GLuint buffer);
void   (APIENTRYP glBufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
void   (APIENTRYP glGenVertexArrays)(GLsizei n, GLuint* arrays);
void   (APIENTRYP glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
void   (APIENTRYP glBindVertexArray)(GLuint array);
void   (APIENTRYP glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
void   (APIENTRYP glEnableVertexAttribArray)(GLuint index);
void   (APIENTRYP glDisableVertexAttribArray)(GLuint index);
void   (APIENTRYP glGetVertexAttribiv)(GLuint index, GLenum pname, GLint* params);
void   (APIENTRYP glDrawArrays)(GLenum mode, GLint first, GLsizei count);
void   (APIENTRYP glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void* indices);

GLuint (APIENTRYP glCreateShader)(GLenum type);
void   (APIENTRYP glDeleteShader)(GLuint shader);
void   (APIENTRYP glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
void   (APIENTRYP glCompileShader)(GLuint shader);
void   (APIENTRYP glGetShaderiv)(GLuint, GLenum, GLint*);
void   (APIENTRYP glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
GLuint (APIENTRYP glCreateProgram)(void);
void   (APIENTRYP glDeleteProgram)(GLuint program);
void   (APIENTRYP glAttachShader)(GLuint program, GLuint shader);
void   (APIENTRYP glLinkProgram)(GLuint program);
void   (APIENTRYP glGetProgramiv)(GLuint, GLenum, GLint*);
void   (APIENTRYP glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
void   (APIENTRYP glUseProgram)(GLuint program);
GLint  (APIENTRYP glGetUniformLocation)(GLuint, const GLchar*);
void   (APIENTRYP glUniform1i)(GLint, GLint);
void   (APIENTRYP glUniform1f)(GLint, GLfloat);
void   (APIENTRYP glUniform2f)(GLint, GLfloat, GLfloat);
void   (APIENTRYP glUniform3f)(GLint, GLfloat, GLfloat, GLfloat);
void   (APIENTRYP glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
void   (APIENTRYP glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);

static GLADloadproc glad_loader = NULL;

static void* Load(const char* name) {
    if (!glad_loader) return NULL;
    return glad_loader(name);
}

#define LOAD(fn) fn = (void*)Load(#fn)

int gladLoadGLLoader(GLADloadproc loader) {
    glad_loader = loader;

    LOAD(glViewport);
    LOAD(glClear);
    LOAD(glClearColor);
    LOAD(glEnable);
    LOAD(glDisable);
    LOAD(glDepthMask);
    LOAD(glCullFace);
    LOAD(glFrontFace);
    LOAD(glBlendFunc);
    LOAD(glGetIntegerv);
    LOAD(glGetString);
    LOAD(glGetError);

    LOAD(glGenTextures);
    LOAD(glDeleteTextures);
    LOAD(glBindTexture);
    LOAD(glTexImage2D);
    LOAD(glTexParameteri);
    LOAD(glGenerateMipmap);
    LOAD(glActiveTexture);

    LOAD(glGenBuffers);
    LOAD(glDeleteBuffers);
    LOAD(glBindBuffer);
    LOAD(glBufferData);
    LOAD(glGenVertexArrays);
    LOAD(glDeleteVertexArrays);
    LOAD(glBindVertexArray);
    LOAD(glVertexAttribPointer);
    LOAD(glEnableVertexAttribArray);
    LOAD(glDisableVertexAttribArray);
    LOAD(glGetVertexAttribiv);
    LOAD(glDrawArrays);
    LOAD(glDrawElements);

    LOAD(glCreateShader);
    LOAD(glDeleteShader);
    LOAD(glShaderSource);
    LOAD(glCompileShader);
    LOAD(glGetShaderiv);
    LOAD(glGetShaderInfoLog);
    LOAD(glCreateProgram);
    LOAD(glDeleteProgram);
    LOAD(glAttachShader);
    LOAD(glLinkProgram);
    LOAD(glGetProgramiv);
    LOAD(glGetProgramInfoLog);
    LOAD(glUseProgram);
    LOAD(glGetUniformLocation);
    LOAD(glUniform1i);
    LOAD(glUniform1f);
    LOAD(glUniform2f);
    LOAD(glUniform3f);
    LOAD(glUniform4f);
    LOAD(glUniformMatrix4fv);

    return glGetString != NULL;
}
