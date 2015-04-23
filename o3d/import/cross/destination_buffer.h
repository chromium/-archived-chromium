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


// This file declares the DestinationBuffer class.

#ifndef O3D_IMPORT_CROSS_DESTINATION_BUFFER_H_
#define O3D_IMPORT_CROSS_DESTINATION_BUFFER_H_

#include "core/cross/buffer.h"

namespace o3d {

// DestinationBuffer is a used for serialization only and is not part of the
// normal O3D plugin. It is used for Skinning to distinguish between a normal
// VertexBuffer that needs to have its contents serialized and a
// DestinationBuffer that only needs to know its structure but not its
// contents.
class DestinationBuffer : public VertexBuffer {
 public:
  typedef SmartPointer<DestinationBuffer> Ref;

  ~DestinationBuffer();

 protected:
  // Overridden from Buffer.
  virtual bool ConcreteAllocate(size_t size_in_bytes);

  // Overridden from Buffer.
  virtual bool ConcreteLock(AccessMode access_mode, void **buffer_data);

  // Overridden from Buffer.
  virtual bool ConcreteUnlock();

  explicit DestinationBuffer(ServiceLocator* service_locator);

 protected:
  // Frees the buffer if it exists.
  void ConcreteFree();

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  scoped_array<char> buffer_;  // The actual data for this buffer.

  O3D_OBJECT_BASE_DECL_CLASS(DestinationBuffer, VertexBuffer);
  DISALLOW_COPY_AND_ASSIGN(DestinationBuffer);
};


}  // namespace o3d

#endif  // O3D_IMPORT_CROSS_DESTINATION_BUFFER_H_

