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


// This file contains the command-buffer versions of VertexBuffer and
// IndexBuffer.

#ifndef O3D_CORE_CROSS_COMMAND_BUFFER_BUFFER_CB_H_
#define O3D_CORE_CROSS_COMMAND_BUFFER_BUFFER_CB_H_

#include "core/cross/precompile.h"
#include "core/cross/buffer.h"
#include "core/cross/command_buffer/renderer_cb.h"

namespace o3d {

// Command-buffer version of VertexBuffer. This class manages the resources for
// vertex buffers and the transfer of data.
// Vertex buffer resources are allocated on the service side (into GPU-friendly
// memory), but the client side cannot map it. So instead, data updates go
// through a buffer in the transfer shared memory when Lock and Unlock are
// called.
class VertexBufferCB : public VertexBuffer {
 public:
  VertexBufferCB(ServiceLocator* service_locator, RendererCB *renderer);
  ~VertexBufferCB();

  // Returns the resource ID for this vertex buffer.
  command_buffer::ResourceID resource_id() const { return resource_id_; }

 protected:
  // Allocates a vertex buffer resource.
  // Parameters:
  //   size_in_bytes: the memory size of the vertex buffer.
  // Returns:
  //   true if successful.
  virtual bool ConcreteAllocate(size_t size_in_bytes);

  // The concrete version of Free
  virtual void ConcreteFree();

  // Locks the vertex buffer for reading and writing. This allocates a buffer
  // into the transfer shared memory. If any data was set into the vertex
  // buffer, this function will copy it back for reading. Otherwise the data in
  // the returned buffer is undefined.
  // Parameters:
  //   access_mode: How you want to access the data.
  //   buffer_data: a pointer to a variable that will contain the pointer to
  //                the locked buffer upon success.
  // Returns:
  //   true if successful.
  virtual bool ConcreteLock(Buffer::AccessMode access_mode, void** buffer_data);

  // Unlocks the vertex buffer, copying the data into the vertex buffer
  // resource.
  // Returns:
  //   true if successful.
  virtual bool ConcreteUnlock();

 private:
  // The pointer to the region in the transfer shared memory buffer when the
  // vertex buffer is locked, or NULL if it is not locked.
  void *lock_pointer_;

  // Whether or not data was entered into the vertex buffer, to avoid copying
  // back undefined pixels.
  bool has_data_;

  // The command buffer resource ID for the vertex buffer.
  command_buffer::ResourceID resource_id_;

  // The renderer that created this vertex buffer.
  RendererCB *renderer_;
};

// Command-buffer version of IndexBuffer. This class manages the resources for
// index buffers and the transfer of data.
// Index buffer resources are allocated on the service side (into GPU-friendly
// memory), but the client side cannot map it. So instead, data updates go
// through a buffer in the transfer shared memory when Lock and Unlock are
// called.
class IndexBufferCB : public IndexBuffer {
 public:
  IndexBufferCB(ServiceLocator* service_locator, RendererCB *renderer);
  ~IndexBufferCB();

  // Returns the resource ID for this vertex buffer.
  command_buffer::ResourceID resource_id() const { return resource_id_; }

 protected:
  // Allocates an index buffer resource.
  // Parameters:
  //   size_in_bytes: the memory size of the index buffer.
  // Returns:
  //   true if successful.
  virtual bool ConcreteAllocate(size_t size_in_bytes);

  // The concrete version of Free
  virtual void ConcreteFree();

  // Locks the index buffer for reading and writing. This allocates a buffer
  // into the transfer shared memory.  If any data was set into the index
  // buffer, this function will copy it back for reading. Otherwise the data in
  // the returned buffer is undefined.
  // Parameters:
  //   access_mode: How you want to access the data.
  //   buffer_data: a pointer to a variable that will contain the pointer to
  //                the locked buffer upon success.
  // Returns:
  //   true if successful.
  virtual bool ConcreteLock(Buffer::AccessMode access_mode, void** buffer_data);

  // Unlocks the index buffer, copying the data into the index buffer
  // resource.
  // Returns:
  //   true if successful.
  virtual bool ConcreteUnlock();

 private:
  // Destroys the resource, and frees the resource ID.
  void Destroy();

  // The pointer to the region in the transfer shared memory buffer when the
  // index buffer is locked, or NULL if it is not locked.
  void *lock_pointer_;

  // Whether or not data was entered into the index buffer, to avoid copying
  // back undefined pixels.
  bool has_data_;

  // The command buffer resource ID for the index buffer.
  command_buffer::ResourceID resource_id_;

  // The renderer that created this index buffer.
  RendererCB *renderer_;
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_COMMAND_BUFFER_BUFFER_CB_H_
