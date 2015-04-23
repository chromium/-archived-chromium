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


// This file declares the VertexBufferGL, IndexBufferGL and VertexStructGL
// classes.

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_GEOMETRY_GL_H_
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_GEOMETRY_GL_H_

#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/service/cross/resource.h"
#include "command_buffer/service/cross/gl/gl_utils.h"

namespace o3d {
namespace command_buffer {

class GAPIGL;

// GL version of VertexBuffer.
class VertexBufferGL : public VertexBuffer {
 public:
  VertexBufferGL(unsigned int size, unsigned int flags)
      : VertexBuffer(size, flags),
        gl_buffer_(0) {}
  virtual ~VertexBufferGL();

  // Creates the GL vertex buffer.
  void Create();

  // Sets the data into the GL vertex buffer.
  bool SetData(unsigned int offset, unsigned int size, const void *data);

  // Gets the data from the GL vertex buffer.
  bool GetData(unsigned int offset, unsigned int size, void *data);

  // Gets the GL vertex buffer.
  GLuint gl_buffer() const { return gl_buffer_; }

 private:
  GLuint gl_buffer_;
  DISALLOW_COPY_AND_ASSIGN(VertexBufferGL);
};

// GL version of IndexBuffer.
class IndexBufferGL : public IndexBuffer {
 public:
  IndexBufferGL(unsigned int size, unsigned int flags)
      : IndexBuffer(size, flags),
        gl_buffer_(0) {}
  virtual ~IndexBufferGL();

  // Creates the GL index buffer.
  void Create();

  // Sets the data into the GL index buffer.
  bool SetData(unsigned int offset, unsigned int size, const void *data);

  // Gets the data from the GL index buffer.
  bool GetData(unsigned int offset, unsigned int size, void *data);

  // Gets the GL index buffer.
  GLuint gl_buffer() const { return gl_buffer_; }

 private:
  GLuint gl_buffer_;
  DISALLOW_COPY_AND_ASSIGN(IndexBufferGL);
};

// GL version of VertexStruct.
class VertexStructGL : public VertexStruct {
 public:
  explicit VertexStructGL(unsigned int count)
      : VertexStruct(count),
        dirty_(true) {}
  virtual ~VertexStructGL() {}

  // Adds an input to the vertex struct.
  void SetInput(unsigned int input_index,
                ResourceID vertex_buffer_id,
                unsigned int offset,
                unsigned int stride,
                vertex_struct::Type type,
                vertex_struct::Semantic semantic,
                unsigned int semantic_index);

  // Sets the input streams to GL.
  unsigned int SetStreams(GAPIGL *gapi);

 private:
  static const unsigned int kMaxAttribs = 16;

  // This struct describes the parameters that are passed to
  // glVertexAttribPointer.
  struct AttribDesc {
    ResourceID vertex_buffer_id;
    GLint size;
    GLenum type;
    GLboolean normalized;
    GLsizei stride;
    GLintptr offset;
  };

  // Compiles the vertex declaration into the attribute array.
  void Compile();

  bool dirty_;
  AttribDesc attribs_[kMaxAttribs];
  DISALLOW_COPY_AND_ASSIGN(VertexStructGL);
};


}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_GEOMETRY_GL_H_
