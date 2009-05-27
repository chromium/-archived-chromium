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


// This file contains the implementatinos of VertexBufferGL and
// IndexBufferGL, used to implement O3D using OpenGL.
//
// To force the vertex and index buffers to be created by Cg Runtime
// control, define the compile flag "USE_CG_BUFFERS". This option is off by
// default and buffers are created, locked and managed using the OpenGL
// "ARB_vertex_buffer_object" extension.

#include "core/cross/precompile.h"
#include "core/cross/error.h"
#include "core/cross/gl/buffer_gl.h"
#include "core/cross/gl/renderer_gl.h"
#include "core/cross/gl/utils_gl.h"
#include "core/cross/gl/utils_gl-inl.h"

namespace o3d {

namespace {

GLenum BufferAccessModeToGLenum(Buffer::AccessMode access_mode) {
  switch (access_mode) {
    case Buffer::READ_ONLY:
      return GL_READ_ONLY_ARB;
    case Buffer::WRITE_ONLY:
      return GL_WRITE_ONLY_ARB;
    case Buffer::READ_WRITE:
      return GL_READ_WRITE_ARB;
  }
  DCHECK(false);
  return GL_READ_WRITE_ARB;
}

}  // anonymous namespace

// Vertex Buffers --------------------------------------------------------------

// Initializes the O3D VertexBuffer object but does not allocate an
// OpenGL vertex buffer object yet.
VertexBufferGL::VertexBufferGL(ServiceLocator* service_locator)
    : VertexBuffer(service_locator),
      renderer_(static_cast<RendererGL*>(
          service_locator->GetService<Renderer>())),
      gl_buffer_(0) {
  DLOG(INFO) << "VertexBufferGL Construct";
}

// Destructor releases the OpenGL VBO.
VertexBufferGL::~VertexBufferGL() {
  DLOG(INFO) << "VertexBufferGL Destruct \"" << name() << "\"";
  ConcreteFree();
}

// Creates a OpenGL vertex buffer of the requested size.
bool VertexBufferGL::ConcreteAllocate(size_t size_in_bytes) {
  DLOG(INFO) << "VertexBufferGL Allocate  \"" << name() << "\"";
  renderer_->MakeCurrentLazy();
  ConcreteFree();
  // Create a new VBO.
  glGenBuffersARB(1, &gl_buffer_);

  if (!gl_buffer_) return false;

  // Give the VBO a size, but no data, and set the hint to "STATIC_DRAW"
  // to mark the buffer as set up once then used often.
  glBindBufferARB(GL_ARRAY_BUFFER_ARB, gl_buffer_);
  glBufferDataARB(GL_ARRAY_BUFFER_ARB,
                  size_in_bytes,
                  NULL,
                  GL_STATIC_DRAW_ARB);
  CHECK_GL_ERROR();
  return true;
}

void VertexBufferGL::ConcreteFree() {
  if (gl_buffer_) {
    renderer_->MakeCurrentLazy();
    glDeleteBuffersARB(1, &gl_buffer_);
    gl_buffer_ = 0;
    CHECK_GL_ERROR();
  }
}

// Calls Lock on the OpenGL buffer to get the address in memory of where the
// buffer data is currently stored.
bool VertexBufferGL::ConcreteLock(Buffer::AccessMode access_mode,
                                  void **buffer_data) {
  DLOG(INFO) << "VertexBufferGL Lock  \"" << name() << "\"";
  renderer_->MakeCurrentLazy();
  glBindBufferARB(GL_ARRAY_BUFFER_ARB, gl_buffer_);
  *buffer_data = glMapBufferARB(GL_ARRAY_BUFFER_ARB,
                                BufferAccessModeToGLenum(access_mode));
  if (*buffer_data == NULL) {
    GLenum error = glGetError();
    if (error == GL_OUT_OF_MEMORY) {
      O3D_ERROR(service_locator()) << "Out of memory for buffer lock.";
    } else {
      O3D_ERROR(service_locator()) << "Unable to lock a GL Array Buffer";
    }
    return false;
  }
  CHECK_GL_ERROR();
  return true;
}

// Calls Unlock on the OpenGL buffer to notify that the contents of the buffer
// are now ready for use.
bool VertexBufferGL::ConcreteUnlock() {
  DLOG(INFO) << "VertexBufferGL Unlock  \"" << name() << "\"";
  renderer_->MakeCurrentLazy();
  glBindBufferARB(GL_ARRAY_BUFFER_ARB, gl_buffer_);
  if (!glUnmapBufferARB(GL_ARRAY_BUFFER)) {
    GLenum error = glGetError();
    if (error == GL_INVALID_OPERATION) {
      O3D_ERROR(service_locator()) <<
          "Buffer was unlocked without first being locked.";
    } else {
      O3D_ERROR(
          service_locator()) << "Unable to unlock a GL Element Array Buffer";
    }
    return false;
  }
  CHECK_GL_ERROR();
  return true;
}


// Index Buffers ---------------------------------------------------------------

// Initializes the O3D IndexBuffer object but does not create a OpenGL
// buffer yet.

IndexBufferGL::IndexBufferGL(ServiceLocator* service_locator)
    : IndexBuffer(service_locator),
      renderer_(static_cast<RendererGL*>(
          service_locator->GetService<Renderer>())),
      gl_buffer_(0) {
  DLOG(INFO) << "IndexBufferGL Construct";
}

// Destructor releases the OpenGL index buffer.
IndexBufferGL::~IndexBufferGL() {
  DLOG(INFO) << "IndexBufferGL Destruct  \"" << name() << "\"";
  ConcreteFree();
}

// Creates a OpenGL index buffer of the requested size.
bool IndexBufferGL::ConcreteAllocate(size_t size_in_bytes) {
  DLOG(INFO) << "IndexBufferGL Allocate  \"" << name() << "\"";
  renderer_->MakeCurrentLazy();
  ConcreteFree();
  // Create a new VBO.
  glGenBuffersARB(1, &gl_buffer_);
  if (!gl_buffer_) return false;
  // Give the VBO a size, but no data, and set the hint to "STATIC_DRAW"
  // to mark the buffer as set up once then used often.
  glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, gl_buffer_);
  glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
                  size_in_bytes,
                  NULL,
                  GL_STATIC_DRAW_ARB);
  CHECK_GL_ERROR();
  return true;
}

void IndexBufferGL::ConcreteFree() {
  if (gl_buffer_) {
    renderer_->MakeCurrentLazy();
    glDeleteBuffersARB(1, &gl_buffer_);
    gl_buffer_ = 0;
    CHECK_GL_ERROR();
  }
}

// Maps the OpenGL buffer to get the address in memory of the buffer data.
bool IndexBufferGL::ConcreteLock(Buffer::AccessMode access_mode,
                                 void **buffer_data) {
  DLOG(INFO) << "IndexBufferGL Lock  \"" << name() << "\"";
  renderer_->MakeCurrentLazy();
  glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, gl_buffer_);
  if (!num_elements())
    return true;
  *buffer_data = glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
                                BufferAccessModeToGLenum(access_mode));
  if (*buffer_data == NULL) {
    GLenum error = glGetError();
    if (error == GL_OUT_OF_MEMORY) {
      O3D_ERROR(service_locator()) << "Out of memory for buffer lock.";
    } else {
      O3D_ERROR(
          service_locator()) << "Unable to lock a GL Element Array Buffer";
    }
    return false;
  }
  CHECK_GL_ERROR();
  return true;
}

// Calls Unlock on the OpenGL buffer to notify that the contents of the buffer
// are now ready for use.
bool IndexBufferGL::ConcreteUnlock() {
  DLOG(INFO) << "IndexBufferGL Unlock  \"" << name() << "\"";
  renderer_->MakeCurrentLazy();
  if (!num_elements())
    return true;
  glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, gl_buffer_);
  if (!glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER)) {
    GLenum error = glGetError();
    if (error == GL_INVALID_OPERATION) {
      O3D_ERROR(service_locator()) <<
          "Buffer was unlocked without first being locked.";
    } else {
      O3D_ERROR(
          service_locator()) << "Unable to unlock a GL Element Array Buffer";
    }
    return false;
  }
  CHECK_GL_ERROR();
  return true;
}
}  // namespace o3d
