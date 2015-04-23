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


// This file contains the implementation of the D3D9 versions of the
// VertexBuffer, IndexBuffer and VertexStruct resources.
// This file also contains the related GAPID3D9 function implementations.

#include <algorithm>
#include "command_buffer/service/win/d3d9/d3d9_utils.h"
#include "command_buffer/service/win/d3d9/geometry_d3d9.h"
#include "command_buffer/service/win/d3d9/gapi_d3d9.h"

namespace o3d {
namespace command_buffer {

// Destroys the D3D9 vertex buffer.
VertexBufferD3D9::~VertexBufferD3D9() {
  DCHECK(d3d_vertex_buffer_ != NULL);
  if (d3d_vertex_buffer_) {
    d3d_vertex_buffer_->Release();
    d3d_vertex_buffer_ = NULL;
  }
}

// Creates a D3D9 vertex buffer.
void VertexBufferD3D9::Create(GAPID3D9 *gapi) {
  DCHECK(d3d_vertex_buffer_ == NULL);

  DWORD d3d_usage = (flags() & vertex_buffer::DYNAMIC) ? D3DUSAGE_DYNAMIC : 0;
  D3DPOOL d3d_pool = D3DPOOL_MANAGED;
  HR(gapi->d3d_device()->CreateVertexBuffer(size(), d3d_usage, 0, d3d_pool,
                                            &d3d_vertex_buffer_, NULL));
}

// Sets the data into the D3D9 vertex buffer, using Lock() and memcpy.
bool VertexBufferD3D9::SetData(unsigned int offset,
                               unsigned int size,
                               const void *data) {
  if (!d3d_vertex_buffer_) {
    LOG(ERROR) << "Calling SetData on a non-initialized VertexBufferD3D9.";
    return false;
  }
  if ((offset >= this->size()) || (offset + size > this->size())) {
    LOG(ERROR) << "Invalid size or offset on VertexBufferD3D9::SetData.";
    return false;
  }
  void *ptr = NULL;
  DWORD lock_flags = 0;
  // If we are setting the full buffer, discard the old data. That's only
  // possible to do for a dynamic d3d vertex buffer.
  if ((offset == 0) && (size == this->size()) &&
      (flags() & vertex_buffer::DYNAMIC))
    lock_flags = D3DLOCK_DISCARD;
  HR(d3d_vertex_buffer_->Lock(offset, size, &ptr, lock_flags));
  memcpy(ptr, data, size);
  HR(d3d_vertex_buffer_->Unlock());
  return true;
}

// Gets the data from the D3D9 vertex buffer, using Lock() and memcpy.
bool VertexBufferD3D9::GetData(unsigned int offset,
                               unsigned int size,
                               void *data) {
  if (!d3d_vertex_buffer_) {
    LOG(ERROR) << "Calling SetData on a non-initialized VertexBufferD3D9.";
    return false;
  }
  if ((offset >= this->size()) || (offset + size > this->size())) {
    LOG(ERROR) << "Invalid size or offset on VertexBufferD3D9::SetData.";
    return false;
  }
  void *ptr = NULL;
  DWORD lock_flags = D3DLOCK_READONLY;
  HR(d3d_vertex_buffer_->Lock(offset, size, &ptr, lock_flags));
  memcpy(data, ptr, size);
  HR(d3d_vertex_buffer_->Unlock());
  return true;
}

// Destroys the D3D9 index buffer.
IndexBufferD3D9::~IndexBufferD3D9() {
  DCHECK(d3d_index_buffer_ != NULL);
  if (d3d_index_buffer_) {
    d3d_index_buffer_->Release();
    d3d_index_buffer_ = NULL;
  }
}

// Creates a D3D9 index buffer.
void IndexBufferD3D9::Create(GAPID3D9 *gapi) {
  DCHECK(d3d_index_buffer_ == NULL);

  DWORD d3d_usage = (flags() & index_buffer::DYNAMIC) ? D3DUSAGE_DYNAMIC : 0;
  D3DFORMAT d3d_format =
      (flags() & index_buffer::INDEX_32BIT) ? D3DFMT_INDEX32 : D3DFMT_INDEX16;
  D3DPOOL d3d_pool = D3DPOOL_MANAGED;
  HR(gapi->d3d_device()->CreateIndexBuffer(size(), d3d_usage, d3d_format,
                                           d3d_pool, &d3d_index_buffer_,
                                           NULL));
}

// Sets the data into the D3D9 index buffer, using Lock() and memcpy.
bool IndexBufferD3D9::SetData(unsigned int offset,
                              unsigned int size,
                              const void *data) {
  if (!d3d_index_buffer_) {
    LOG(ERROR) << "Calling SetData on a non-initialized IndexBufferD3D9.";
    return false;
  }
  if ((offset >= this->size()) || (offset + size > this->size())) {
    LOG(ERROR) << "Invalid size or offset on IndexBufferD3D9::SetData.";
    return false;
  }
  void *ptr = NULL;
  DWORD lock_flags = 0;
  // If we are setting the full buffer, discard the old data. That's only
  // possible to do for a dynamic d3d index buffer.
  if ((offset == 0) && (size == this->size()) &&
      (flags() & index_buffer::DYNAMIC))
    lock_flags = D3DLOCK_DISCARD;
  HR(d3d_index_buffer_->Lock(offset, size, &ptr, lock_flags));
  memcpy(ptr, data, size);
  HR(d3d_index_buffer_->Unlock());
  return true;
}

// Gets the data from the D3D9 index buffer, using Lock() and memcpy.
bool IndexBufferD3D9::GetData(unsigned int offset,
                              unsigned int size,
                              void *data) {
  if (!d3d_index_buffer_) {
    LOG(ERROR) << "Calling SetData on a non-initialized IndexBufferD3D9.";
    return false;
  }
  if ((offset >= this->size()) || (offset + size > this->size())) {
    LOG(ERROR) << "Invalid size or offset on IndexBufferD3D9::SetData.";
    return false;
  }
  void *ptr = NULL;
  DWORD lock_flags = D3DLOCK_READONLY;
  HR(d3d_index_buffer_->Lock(offset, size, &ptr, lock_flags));
  memcpy(data, ptr, size);
  HR(d3d_index_buffer_->Unlock());
  return true;
}

// Sets the input element in the VertexStruct resource.
void VertexStructD3D9::SetInput(unsigned int input_index,
                                ResourceID vertex_buffer_id,
                                unsigned int offset,
                                unsigned int stride,
                                vertex_struct::Type type,
                                vertex_struct::Semantic semantic,
                                unsigned int semantic_index) {
  Element &element = GetElement(input_index);
  element.vertex_buffer = vertex_buffer_id;
  element.offset = offset;
  element.stride = stride;
  element.type = type;
  element.semantic = semantic;
  element.semantic_index = semantic_index;
  dirty_ = true;
}

// Sets the vdecl and stream sources in D3D9. Compiles them if needed.
unsigned int VertexStructD3D9::SetStreams(GAPID3D9 *gapi) {
  IDirect3DDevice9 *d3d_device = gapi->d3d_device();
  if (dirty_) Compile(d3d_device);
  HR(d3d_device->SetVertexDeclaration(d3d_vertex_decl_));
  unsigned int max_vertices = UINT_MAX;
  for (unsigned int i = 0; i < streams_.size(); ++i) {
    const StreamPair &pair = streams_[i];
    VertexBufferD3D9 *vertex_buffer = gapi->GetVertexBuffer(pair.first);
    if (!vertex_buffer) {
      max_vertices = 0;
      continue;
    }
    HR(d3d_device->SetStreamSource(i, vertex_buffer->d3d_vertex_buffer(), 0,
                                   pair.second));
    max_vertices = std::min(max_vertices, vertex_buffer->size()/pair.second);
  }
  return max_vertices;
}

// Converts a vertex_struct::Type to a D3DDECLTYPE.
static D3DDECLTYPE D3DType(vertex_struct::Type type) {
  switch (type) {
    case vertex_struct::FLOAT1:
      return D3DDECLTYPE_FLOAT1;
    case vertex_struct::FLOAT2:
      return D3DDECLTYPE_FLOAT2;
    case vertex_struct::FLOAT3:
      return D3DDECLTYPE_FLOAT3;
    case vertex_struct::FLOAT4:
      return D3DDECLTYPE_FLOAT4;
    case vertex_struct::UCHAR4N:
      return D3DDECLTYPE_UBYTE4N;
    case vertex_struct::NUM_TYPES:
      break;
  }
  LOG(FATAL) << "Invalid type";
  return D3DDECLTYPE_FLOAT1;
}

// Converts a vertex_struct::Semantic to a D3DDECLUSAGE.
static D3DDECLUSAGE D3DUsage(vertex_struct::Semantic semantic) {
  switch (semantic) {
    case vertex_struct::POSITION:
      return D3DDECLUSAGE_POSITION;
    case vertex_struct::NORMAL:
      return D3DDECLUSAGE_NORMAL;
    case vertex_struct::COLOR:
      return D3DDECLUSAGE_COLOR;
    case vertex_struct::TEX_COORD:
      return D3DDECLUSAGE_TEXCOORD;
    case vertex_struct::NUM_SEMANTICS:
      break;
  }
  LOG(FATAL) << "Invalid type";
  return D3DDECLUSAGE_POSITION;
}

// Destroys the d3d vertex declaration.
VertexStructD3D9::~VertexStructD3D9() {
  Destroy();
}

void VertexStructD3D9::Destroy() {
  if (d3d_vertex_decl_) {
    d3d_vertex_decl_->Release();
    d3d_vertex_decl_ = NULL;
  }
  streams_.clear();
}

// Compiles a stream map and a d3d vertex declaration from the list of inputs.
// 2 inputs that use the same vertex buffer and stride will use the same
// d3d stream.
void VertexStructD3D9::Compile(IDirect3DDevice9 *d3d_device) {
  DCHECK(dirty_);
  Destroy();
  streams_.reserve(count_);
  scoped_array<D3DVERTEXELEMENT9> d3d_elements(
      new D3DVERTEXELEMENT9[count_ + 1]);
  memset(d3d_elements.get(), 0, sizeof(D3DVERTEXELEMENT9) * (count_ + 1));
  // build streams_ like a set, but the order matters.
  for (unsigned int i = 0; i < count_ ; ++i) {
    Element &element = GetElement(i);
    D3DVERTEXELEMENT9 &d3d_element = d3d_elements[i];
    StreamPair pair(element.vertex_buffer, element.stride);
    std::vector<StreamPair>::iterator it =
        std::find(streams_.begin(), streams_.end(), pair);
    unsigned int stream_index = 0;
    if (it == streams_.end()) {
      streams_.push_back(pair);
      stream_index = static_cast<unsigned int>(streams_.size() - 1);
    } else {
      stream_index = it - streams_.begin();
    }
    d3d_element.Stream = stream_index;
    d3d_element.Offset = element.offset;
    d3d_element.Type = D3DType(element.type);
    d3d_element.Usage = D3DUsage(element.semantic);
    d3d_element.UsageIndex = element.semantic_index;
  }
  D3DVERTEXELEMENT9 &end = d3d_elements[count_];
  end.Stream = 0xFF;
  end.Type = D3DDECLTYPE_UNUSED;
  HR(d3d_device->CreateVertexDeclaration(d3d_elements.get(),
                                         &d3d_vertex_decl_));
  dirty_ = false;
}

// Creates and assigns a VertexBufferD3D9 resource.
BufferSyncInterface::ParseError GAPID3D9::CreateVertexBuffer(
    ResourceID id,
    unsigned int size,
    unsigned int flags) {
  VertexBufferD3D9 *vertex_buffer = new VertexBufferD3D9(size, flags);
  vertex_buffer->Create(this);
  vertex_buffers_.Assign(id, vertex_buffer);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

// Destroys a VertexBufferD3D9 resource.
BufferSyncInterface::ParseError GAPID3D9::DestroyVertexBuffer(ResourceID id) {
  return vertex_buffers_.Destroy(id) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

// Copies the data into the VertexBufferD3D9 resource.
BufferSyncInterface::ParseError GAPID3D9::SetVertexBufferData(
    ResourceID id,
    unsigned int offset,
    unsigned int size,
    const void *data) {
  VertexBufferD3D9 *vertex_buffer = vertex_buffers_.Get(id);
  if (!vertex_buffer) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return vertex_buffer->SetData(offset, size, data) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

// Copies the data from the VertexBufferD3D9 resource.
BufferSyncInterface::ParseError GAPID3D9::GetVertexBufferData(
    ResourceID id,
    unsigned int offset,
    unsigned int size,
    void *data) {
  VertexBufferD3D9 *vertex_buffer = vertex_buffers_.Get(id);
  if (!vertex_buffer) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return vertex_buffer->GetData(offset, size, data) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

// Creates and assigns an IndexBufferD3D9 resource.
BufferSyncInterface::ParseError GAPID3D9::CreateIndexBuffer(
    ResourceID id,
    unsigned int size,
    unsigned int flags) {
  IndexBufferD3D9 *index_buffer = new IndexBufferD3D9(size, flags);
  index_buffer->Create(this);
  index_buffers_.Assign(id, index_buffer);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

// Destroys an IndexBufferD3D9 resource.
BufferSyncInterface::ParseError GAPID3D9::DestroyIndexBuffer(ResourceID id) {
  return index_buffers_.Destroy(id) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

// Copies the data into the IndexBufferD3D9 resource.
BufferSyncInterface::ParseError GAPID3D9::SetIndexBufferData(
    ResourceID id,
    unsigned int offset,
    unsigned int size,
    const void *data) {
  IndexBufferD3D9 *index_buffer = index_buffers_.Get(id);
  if (!index_buffer) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return index_buffer->SetData(offset, size, data) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

// Copies the data from the IndexBufferD3D9 resource.
BufferSyncInterface::ParseError GAPID3D9::GetIndexBufferData(
    ResourceID id,
    unsigned int offset,
    unsigned int size,
    void *data) {
  IndexBufferD3D9 *index_buffer = index_buffers_.Get(id);
  if (!index_buffer) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return index_buffer->GetData(offset, size, data) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

// Creates and assigns a VertexStructD3D9 resource.
BufferSyncInterface::ParseError GAPID3D9::CreateVertexStruct(
    ResourceID id, unsigned int input_count) {
  if (id == current_vertex_struct_) validate_streams_ = true;
  VertexStructD3D9 *vertex_struct = new VertexStructD3D9(input_count);
  vertex_structs_.Assign(id, vertex_struct);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

// Destroys a VertexStructD3D9 resource.
BufferSyncInterface::ParseError GAPID3D9::DestroyVertexStruct(ResourceID id) {
  if (id == current_vertex_struct_) validate_streams_ = true;
  return vertex_structs_.Destroy(id) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

// Sets an input into a VertexStructD3D9 resource.
BufferSyncInterface::ParseError GAPID3D9::SetVertexInput(
    ResourceID vertex_struct_id,
    unsigned int input_index,
    ResourceID vertex_buffer_id,
    unsigned int offset,
    unsigned int stride,
    vertex_struct::Type type,
    vertex_struct::Semantic semantic,
    unsigned int semantic_index) {
  if (vertex_buffer_id == current_vertex_struct_) validate_streams_ = true;
  VertexStructD3D9 *vertex_struct = vertex_structs_.Get(vertex_struct_id);
  if (!vertex_struct || input_index >= vertex_struct->count())
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  vertex_struct->SetInput(input_index, vertex_buffer_id, offset, stride, type,
                          semantic, semantic_index);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

}  // namespace command_buffer
}  // namespace o3d
