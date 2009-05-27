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


// This file contains the definition of the D3D9 versions of geometry-related
// resource classes.

#ifndef O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_GEOMETRY_D3D9_H__
#define O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_GEOMETRY_D3D9_H__

#include <vector>
#include <utility>
#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/service/win/d3d9/d3d9_utils.h"
#include "command_buffer/service/cross/resource.h"

namespace o3d {
namespace command_buffer {

class GAPID3D9;

// D3D9 version of VertexBuffer.
class VertexBufferD3D9 : public VertexBuffer {
 public:
  VertexBufferD3D9(unsigned int size, unsigned int flags)
      : VertexBuffer(size, flags),
        d3d_vertex_buffer_(NULL) {}
  virtual ~VertexBufferD3D9();
  // Creates the D3D vertex buffer.
  void Create(GAPID3D9 *gapi);
  // Sets the data into the D3D vertex buffer.
  bool SetData(unsigned int offset, unsigned int size, const void *data);
  // Gets the data from the D3D vertex buffer.
  bool GetData(unsigned int offset, unsigned int size, void *data);

  // Gets the D3D vertex buffer.
  IDirect3DVertexBuffer9 * d3d_vertex_buffer() { return d3d_vertex_buffer_; }
 private:
  IDirect3DVertexBuffer9 *d3d_vertex_buffer_;
  DISALLOW_COPY_AND_ASSIGN(VertexBufferD3D9);
};

// D3D9 version of IndexBuffer.
class IndexBufferD3D9 : public IndexBuffer {
 public:
  IndexBufferD3D9(unsigned int size, unsigned int flags)
      : IndexBuffer(size, flags),
        d3d_index_buffer_(NULL) {}
  virtual ~IndexBufferD3D9();
  // Creates the D3D index buffer.
  void Create(GAPID3D9 *gapi);
  // Sets the data into the D3D index buffer.
  bool SetData(unsigned int offset, unsigned int size, const void *data);
  // Gets the data from the D3D index buffer.
  bool GetData(unsigned int offset, unsigned int size, void *data);

  // Gets the D3D index buffer.
  IDirect3DIndexBuffer9 *d3d_index_buffer() const { return d3d_index_buffer_; }
 private:
  IDirect3DIndexBuffer9 *d3d_index_buffer_;
  DISALLOW_COPY_AND_ASSIGN(IndexBufferD3D9);
};

// D3D9 version of VertexStruct.
class VertexStructD3D9 : public VertexStruct {
 public:
  explicit VertexStructD3D9(unsigned int count)
      : VertexStruct(count),
        dirty_(true),
        d3d_vertex_decl_(NULL) {}
  virtual ~VertexStructD3D9();
  // Adds an input to the vertex struct.
  void SetInput(unsigned int input_index,
                ResourceID vertex_buffer_id,
                unsigned int offset,
                unsigned int stride,
                vertex_struct::Type type,
                vertex_struct::Semantic semantic,
                unsigned int semantic_index);
  // Sets the input streams to D3D.
  unsigned int SetStreams(GAPID3D9 *gapi);
 private:
  // Destroys the vertex declaration and stream map.
  void Destroy();
  // Compiles the vertex declaration and stream map.
  void Compile(IDirect3DDevice9 *d3d_device);

  bool dirty_;
  typedef std::pair<ResourceID, unsigned int> StreamPair;
  std::vector<StreamPair> streams_;
  IDirect3DVertexDeclaration9 *d3d_vertex_decl_;
  DISALLOW_COPY_AND_ASSIGN(VertexStructD3D9);
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_GEOMETRY_D3D9_H__
