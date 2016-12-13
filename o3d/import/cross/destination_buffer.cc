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

#include "import/cross/destination_buffer.h"

namespace o3d {

O3D_OBJECT_BASE_DEFN_CLASS(
    "o3djs.DestinationBuffer", DestinationBuffer, VertexBuffer);

DestinationBuffer::DestinationBuffer(ServiceLocator* service_locator)
    : VertexBuffer(service_locator),
      buffer_() {
}

DestinationBuffer::~DestinationBuffer() {
  ConcreteFree();
}

void DestinationBuffer::ConcreteFree() {
  buffer_.reset();
}

bool DestinationBuffer::ConcreteAllocate(size_t size_in_bytes) {
  ConcreteFree();

  buffer_.reset(new char[size_in_bytes]);

  return true;
}

bool DestinationBuffer::ConcreteLock(AccessMode access_mode,
                                     void **buffer_data) {
  if (!buffer_.get()) {
    return false;
  }

  *buffer_data = reinterpret_cast<void*>(buffer_.get());
  return true;
}

bool DestinationBuffer::ConcreteUnlock() {
  return buffer_.get() != NULL;
}

ObjectBase::Ref DestinationBuffer::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new DestinationBuffer(service_locator));
}

}  // namespace o3d

