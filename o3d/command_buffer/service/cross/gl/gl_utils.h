/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file includes all the necessary GL/Cg headers and implement some useful
// utilities.

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_GL_UTILS_H_
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_GL_UTILS_H_

#include <build/build_config.h>
#define GL_GLEXT_PROTOTYPES
#if defined(OS_WIN)
#include <GL/gl.h>
#elif defined(OS_MACOSX)
#include <OpenGL/OpenGL.h>
#include <AGL/agl.h>
#elif defined(OS_LINUX)
#include <GL/gl.h>
#endif  // defined(PLATFORM)
#include <Cg/cg.h>
#include <Cg/cgGL.h>

// Define this for extra GL error debugging (slower).
// #define GL_ERROR_DEBUGGING
#ifdef GL_ERROR_DEBUGGING
#define CHECK_GL_ERROR() do {                                           \
    GLenum gl_error = glGetError();                                     \
    LOG_IF(ERROR, gl_error != GL_NO_ERROR) << "GL Error :" << gl_error; \
  } while (0)
#else  // GL_ERROR_DEBUGGING
#define CHECK_GL_ERROR() void(0)
#endif  // GL_ERROR_DEBUGGING

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_GL_UTILS_H_
