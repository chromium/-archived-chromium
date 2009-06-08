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


// This file contains the declaration of VertexBufferD3D9 and IndexBufferD3D9.

#ifndef O3D_CORE_WIN_D3D9_BUFFER_D3D9_H_
#define O3D_CORE_WIN_D3D9_BUFFER_D3D9_H_

#include <d3d9.h>
#include <atlbase.h>
#include "base/scoped_ptr.h"
#include "core/cross/buffer.h"

namespace o3d {

// VertexBufferD3D9 is a wrapper around the DX9 vertex buffer.
// The buffer starts out empty.  Calling Allocate will reserve video memory
// for the buffer. Buffer contents are are updated by calling Lock() to get a
// pointer to the memory allocated for the buffer, updating that data in place
// and calling Unlock() to notify DX that the edits are done.
class VertexBufferD3D9 : public VertexBuffer {
 public:
  VertexBufferD3D9(ServiceLocator* service_locator,
                   IDirect3DDevice9* d3d_device);
  ~VertexBufferD3D9();

  // Returns the DX9 vertex buffer handle.
  LPDIRECT3DVERTEXBUFFER9 d3d_buffer() const { return d3d_buffer_; }

 protected:
  // Creates a DX9 vertex buffer of the specified size.
  virtual bool ConcreteAllocate(size_t size_in_bytes);

  // Frees the buffer.
  virtual void ConcreteFree();

  // Returns a pointer to the current contents of the buffer.  A matching
  // call to Unlock is necessary to update the contents of the buffer.
  virtual bool ConcreteLock(AccessMode access_mode, void** buffer_data);

  // Notifies DX9 that the buffer data has been updated.  Unlock is only valid
  // if it follows a Lock operation.
  virtual bool ConcreteUnlock();

 private:
  // The D3D Device interface used to create this object.
  CComPtr<IDirect3DDevice9> d3d_device_;
  // Pointer to the internal D3D9 Vertex Buffer object.
  CComPtr<IDirect3DVertexBuffer9> d3d_buffer_;
};

// IndexBufferD3D9 is a wrapper around the DX9 index buffer object.  The buffer
// starts out empty.  A call to Allocate will create a DX9 index buffer of the
// requested size.  Updates the to the contents of the buffer are done via
// the Lock/Unlock calls.
class IndexBufferD3D9 : public IndexBuffer {
 public:
  IndexBufferD3D9(ServiceLocator* service_locator,
                  IDirect3DDevice9* d3d_device,
                  bool small_buffer);
  ~IndexBufferD3D9();

  // Returns the DX9 index buffer handle.
  inline LPDIRECT3DINDEXBUFFER9 d3d_buffer() const { return d3d_buffer_; }

 protected:
  // Creates a DX9 index buffer of the specified size.
  virtual bool ConcreteAllocate(size_t size_in_bytes);

  // Frees the buffer.
  virtual void ConcreteFree();

  // Returns a pointer to the current contents of the buffer.  After calling
  // Lock, the contents of the buffer can be updated in place.
  virtual bool ConcreteLock(AccessMode access_mode, void** buffer_data);

  // Notifies DX9 that the buffer data has been updated.  Unlock is only
  // valid if it follows a Lock operation.
  virtual bool ConcreteUnlock();

 private:
  bool dirty_;
  bool small_;      // 16 or 32 bit. True = 16bit.
  scoped_array<uint8> shadow_buffer_;  // shadow buffer if this buffer is small
  // The D3D Device interface used to create this object.
  CComPtr<IDirect3DDevice9> d3d_device_;
  // Pointer to the internal D3D9 Index Buffer object.
  CComPtr<IDirect3DIndexBuffer9> d3d_buffer_;
};

}  // namespace o3d


#endif  // O3D_CORE_WIN_D3D9_BUFFER_D3D9_H_
