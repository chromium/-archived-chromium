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
// VertexBufferStub and IndexBufferStub objects used by O3D

#ifndef O3D_CONVERTER_CROSS_BUFFER_STUB_H_
#define O3D_CONVERTER_CROSS_BUFFER_STUB_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "core/cross/buffer.h"

namespace o3d {

// VertexBufferStub is a wrapper around an Stub Vertex Buffer Object (VBO).
// The buffer starts out empty.  Calling Allocate() will reserve video memory
// for the buffer. Buffer contents are are updated by calling Lock() to get a
// pointer to the memory allocated for the buffer, updating that data in place
// and calling Unlock() to notify Stub that the edits are done.

class VertexBufferStub : public VertexBuffer {
 public:
  explicit VertexBufferStub(ServiceLocator* service_locator)
      : VertexBuffer(service_locator), locked_(false) {}
  ~VertexBufferStub() {}

 protected:
  // Creates a Stub vertex buffer object of the specified size.
  virtual bool ConcreteAllocate(size_t size_in_bytes);

  // Frees the buffer.
  virtual void ConcreteFree();

  // Returns a pointer to the current contents of the buffer.  A matching
  // call to Unlock is necessary to update the contents of the buffer.
  virtual bool ConcreteLock(AccessMode access_mode, void** buffer_data);

  // Notifies Stub that the buffer data has been updated.  Unlock is only
  // valid if it follows a Lock operation.
  virtual bool ConcreteUnlock();
 private:
  scoped_array<int8> buffer_;
  bool locked_;

  DISALLOW_COPY_AND_ASSIGN(VertexBufferStub);
};

// IndexBufferStub is a wrapper around a Stub Index Buffer Object.
// The buffer starts out empty.  A call to Allocate() will create a stub
// index buffer of the requested size.  Updates the to the contents of the
// buffer are done via the Lock/Unlock calls.
class IndexBufferStub : public IndexBuffer {
 public:
  explicit IndexBufferStub(ServiceLocator* service_locator)
      : IndexBuffer(service_locator), locked_(false) {}
  ~IndexBufferStub() {}

 protected:
  // Creates a OpenGL index buffer of the specified size.
  virtual bool ConcreteAllocate(size_t size_in_bytes);

  // Frees the buffer.
  virtual void ConcreteFree();

  // Returns a pointer to the current contents of the buffer.  After calling
  // Lock, the contents of the buffer can be updated in place.
  virtual bool ConcreteLock(AccessMode access_mode, void** buffer_data);

  // Notifies OpenGL that the buffer data has been updated.  Unlock is only
  // valid if it follows a Lock operation.
  virtual bool ConcreteUnlock();
 private:
  scoped_array<int8> buffer_;
  bool locked_;

  DISALLOW_COPY_AND_ASSIGN(IndexBufferStub);
};

}  // namespace o3d


#endif  // O3D_CONVERTER_CROSS_BUFFER_STUB_H_
