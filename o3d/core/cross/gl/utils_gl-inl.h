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


#ifndef O3D_CORE_CROSS_GL_UTILS_GL_INL_H_
#define O3D_CORE_CROSS_GL_UTILS_GL_INL_H_

#include "core/cross/precompile.h"
#include "core/cross/types.h"

namespace o3d {

// Define this to debug GL errors. This has a significant performance hit.
// #define GL_ERROR_DEBUGGING

// convert a byte offset into a Vertex Buffer Object into a GLvoid* for
// use with glVertexPointer(), glNormalPointer(), glVertexAttribPointer(),
// etc. after having used a glBindBuffer().
#define BUFFER_OFFSET(i) (reinterpret_cast<char *>(NULL)+(i))

// Writes any Cg errors to the log with a descriptive message.
// NOTE: macros used to make sure the LOG calls note the correct
// line number and source file.
#define DLOG_CG_ERROR(message) {                \
  CGerror error = cgGetError();                 \
  DLOG_IF(INFO, error != CG_NO_ERROR)           \
      << message << " : "                       \
      << cgGetErrorString(error);               \
}

// Writes any Cg errors to the log with a descriptive message, along with
// the error messages from the CGC compiler.
#define DLOG_CG_COMPILER_ERROR(message, cg_context) { \
  CGerror error = cgGetError();                       \
  DLOG_IF(ERROR, error == CG_NO_ERROR)                \
      << message << " : " << cgGetErrorString(error); \
  if (error == CG_COMPILER_ERROR) {                   \
    DLOG(ERROR) << "CGC compiler output :\n"          \
                << cgGetLastListing(cg_context);      \
  }                                                   \
}

#ifdef GL_ERROR_DEBUGGING
#define CHECK_GL_ERROR() do {                                         \
  GLenum gl_error = glGetError();                                     \
  LOG_IF(ERROR, gl_error != GL_NO_ERROR) << "GL Error :" << gl_error; \
} while(0)
#else  // GL_ERROR_DEBUGGING
#define CHECK_GL_ERROR() void(0)
#endif  // GL_ERROR_DEBUGGING

}  // namespace o3d

#endif  // O3D_CORE_CROSS_GL_UTILS_GL_INL_H_
