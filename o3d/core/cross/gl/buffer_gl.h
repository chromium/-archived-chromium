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


// This file contains the declaration of the platform specific
// VertexBufferGL and IndexBufferGL objects used by O3D

#ifndef O3D_CORE_CROSS_GL_BUFFER_GL_H_
#define O3D_CORE_CROSS_GL_BUFFER_GL_H_

#include "core/cross/precompile.h"
#include "core/cross/buffer.h"

namespace o3d {

class RendererGL;

// VertexBufferGL is a wrapper around an OpenGL Vertex Buffer Object (VBO).
// The buffer starts out empty.  Calling Allocate() will reserve video memory
// for the buffer. Buffer contents are are updated by calling Lock() to get a
// pointer to the memory allocated for the buffer, updating that data in place
// and calling Unlock() to notify OpenGL that the edits are done.
//
// To force the vertex and index buffers to be created by Cg Runtime
// control, define the compile flag "USE_CG_BUFFERS". This option is off by
// default and buffers are created, locked and managed using the OpenGL
// "ARB_vertex_buffer_object" extension.

class VertexBufferGL : public VertexBuffer {
 public:
  explicit VertexBufferGL(ServiceLocator* service_locator);
  ~VertexBufferGL();

  // Returns the OpenGL vertex buffer Object handle.
  GLuint gl_buffer() const { return gl_buffer_; }

 protected:
  // Creates a OpenGL vertex buffer object of the specified size.
  virtual bool ConcreteAllocate(size_t size_in_bytes);

  // Frees the OpenGL vertex buffer object.
  virtual void ConcreteFree();

  // Returns a pointer to the current contents of the buffer.  A matching
  // call to Unlock is necessary to update the contents of the buffer.
  virtual bool ConcreteLock(AccessMode access_mode, void** buffer_data);

  // Notifies OpenGL that the buffer data has been updated.  Unlock is only
  // valid if it follows a Lock operation.
  virtual bool ConcreteUnlock();

 private:
  RendererGL* renderer_;
  GLuint gl_buffer_;
};

// IndexBufferGL is a wrapper around an OpenGL Index Buffer Object (VBO).
// The buffer starts out empty.  A call to Allocate() will create an OpenGL
// index buffer of the requested size.  Updates the to the contents of the
// buffer are done via the Lock/Unlock calls.
class IndexBufferGL : public IndexBuffer {
 public:
  explicit IndexBufferGL(ServiceLocator* service_locator);
  ~IndexBufferGL();

  // Returns the OpenGL vertex buffer Object handle.
  GLuint gl_buffer() const { return gl_buffer_; }

 protected:
  // Creates a OpenGL index buffer of the specified size.
  virtual bool ConcreteAllocate(size_t size_in_bytes);

  // Frees the OpenGL vertex buffer object.
  virtual void ConcreteFree();

  // Returns a pointer to the current contents of the buffer.  After calling
  // Lock, the contents of the buffer can be updated in place.
  virtual bool ConcreteLock(AccessMode access_mode, void** buffer_data);

  // Notifies OpenGL that the buffer data has been updated.  Unlock is only
  // valid if it follows a Lock operation.
  virtual bool ConcreteUnlock();

 private:
  RendererGL* renderer_;
  GLuint gl_buffer_;
};

}  // namespace o3d


#endif  // O3D_CORE_CROSS_GL_BUFFER_GL_H_
