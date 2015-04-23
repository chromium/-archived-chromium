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


// This file contains the definition of the VertexBufferStub and
// IndexBufferStub classes.

#include "converter/cross/buffer_stub.h"

namespace o3d {

bool VertexBufferStub::ConcreteAllocate(size_t size_in_bytes) {
  buffer_.reset(new int8[size_in_bytes]);
  return true;
}

void VertexBufferStub::ConcreteFree() {
  buffer_.reset();
}

bool VertexBufferStub::ConcreteLock(AccessMode access_mode,
                                    void** buffer_data) {
  *buffer_data = buffer_.get();
  DCHECK(locked_ == false);
  locked_ = true;
  return (buffer_.get() != NULL);
}

bool VertexBufferStub::ConcreteUnlock() {
  bool status = locked_;
  DCHECK(locked_ == true);
  locked_ = false;
  return status;
}

bool IndexBufferStub::ConcreteAllocate(size_t size_in_bytes) {
  buffer_.reset(new int8[size_in_bytes]);
  return true;
}

void IndexBufferStub::ConcreteFree() {
  buffer_.reset();
}

bool IndexBufferStub::ConcreteLock(AccessMode access_mode, void** buffer_data) {
  *buffer_data = buffer_.get();
  DCHECK(locked_ == false);
  locked_ = true;
  return (buffer_.get() != NULL);
}

bool IndexBufferStub::ConcreteUnlock() {
  bool status = locked_;
  DCHECK(locked_ == true);
  locked_ = false;
  return status;
}

}  // end namespace o3d
