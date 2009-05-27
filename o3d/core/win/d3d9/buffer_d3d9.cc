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


// This file contains the definitions of VertexBufferD3D9 and IndexBufferD3D9.

#include "core/cross/precompile.h"
#include "core/win/d3d9/buffer_d3d9.h"
#include "core/win/d3d9/utils_d3d9.h"

namespace o3d {

namespace {

DWORD BufferAccessModeToD3DLock(Buffer::AccessMode access_mode) {
  switch (access_mode) {
    case Buffer::READ_ONLY:
      return D3DLOCK_READONLY;
      break;
    case Buffer::WRITE_ONLY:
      return 0;
    case Buffer::READ_WRITE:
      return 0;
  }
  DCHECK(false);
  return 0;
}

}  // anonymous namespace

// Initializes the O3D VertexBuffer object but does not create a DX9 buffer
// yet.
VertexBufferD3D9::VertexBufferD3D9(ServiceLocator* service_locator,
                                   LPDIRECT3DDEVICE9 d3d_device)
    : VertexBuffer(service_locator),
      d3d_device_(d3d_device) {
  DCHECK(d3d_device);
}

VertexBufferD3D9::~VertexBufferD3D9() {
}

// Creates a DX9 vertex buffer of the requested size.
bool VertexBufferD3D9::ConcreteAllocate(unsigned int size_in_bytes) {
  d3d_buffer_ = NULL;
  return HR(d3d_device_->CreateVertexBuffer(
      size_in_bytes,
      0,
      0,
      D3DPOOL_MANAGED,
      &d3d_buffer_,
      NULL));
}

void VertexBufferD3D9::ConcreteFree() {
  d3d_buffer_ = NULL;
}

// Calls Lock on the DX9 buffer to get the address in memory of where the
// buffer data is currently stored.
bool VertexBufferD3D9::ConcreteLock(AccessMode access_mode,
                                    void **buffer_data) {
  *buffer_data = NULL;

  if (GetSizeInBytes() == 0)
    return true;

  if (!d3d_buffer_)
    return false;

  if (!HR(d3d_buffer_->Lock(0, GetSizeInBytes(), buffer_data,
                            BufferAccessModeToD3DLock(access_mode)))) {
    return false;
  }

  // Yes, this really happens sometimes.
  if (NULL == *buffer_data) {
    HR(d3d_buffer_->Unlock());
    return false;
  }

  return true;
}

// Calls Unlock on the DX9 buffer to notify that the contents of the buffer
// are now ready for use.
bool VertexBufferD3D9::ConcreteUnlock() {
  if (!d3d_buffer_) return false;
  return HR(d3d_buffer_->Unlock());
}

// Initializes the O3D IndexBuffer object but does not create a DX9 buffer
// yet.
IndexBufferD3D9::IndexBufferD3D9(ServiceLocator* service_locator,
                                 LPDIRECT3DDEVICE9 d3d_device,
                                 bool small_buffer)
    : IndexBuffer(service_locator),
      d3d_device_(d3d_device),
      shadow_buffer_(NULL),
      dirty_(false),
      small_(small_buffer) {
  DCHECK(d3d_device);
}

IndexBufferD3D9::~IndexBufferD3D9() {
}

// Creates a DX9 index buffer of the requested size.
bool IndexBufferD3D9::ConcreteAllocate(unsigned int size_in_bytes) {
  d3d_buffer_ = NULL;
  if (small_) {
    shadow_buffer_.reset(new uint8[size_in_bytes]);
  }
  return HR(d3d_device_->CreateIndexBuffer(
      size_in_bytes / (small_ ? 2 : 1),
      0,
      small_ ? D3DFMT_INDEX16 : D3DFMT_INDEX32,
      D3DPOOL_MANAGED,
      &d3d_buffer_,
      NULL));
}

void IndexBufferD3D9::ConcreteFree() {
  d3d_buffer_ = NULL;
  shadow_buffer_.reset();
}

// Calls Lock on the DX9 buffer to get the address in memory of where the
// buffer data is currently stored.
bool IndexBufferD3D9::ConcreteLock(AccessMode access_mode, void **buffer_data) {
  *buffer_data = NULL;

  if (GetSizeInBytes() == 0)
    return true;

  if (!d3d_buffer_)
    return false;

  if (small_) {
    *buffer_data = shadow_buffer_.get();
    if (access_mode == Buffer::READ_WRITE ||
        access_mode == Buffer::WRITE_ONLY) {
      dirty_ = true;
    }
    return true;
  }

  if (!HR(d3d_buffer_->Lock(0, GetSizeInBytes(), buffer_data,
                            BufferAccessModeToD3DLock(access_mode)))) {
    return false;
  }

  // Yes, this really happens sometimes.
  // WTF is this? If this fails the app fails!
  if (NULL == *buffer_data) {
    HR(d3d_buffer_->Unlock());
    return false;
  }

  return true;
}

// Calls Unlock on the DX9 buffer to notify that the contents of the buffer
// are now ready for use.
bool IndexBufferD3D9::ConcreteUnlock() {
  if (GetSizeInBytes() == 0) return true;
  if (!d3d_buffer_) return false;
  if (small_) {
    // TODO: Move this to just before rendering.
    if (dirty_) {
      void* data;
      bool locked = HR(d3d_buffer_->Lock(0, GetSizeInBytes() / 2, &data,
                                         Buffer::WRITE_ONLY));
      if (locked) {
        uint16* destination = reinterpret_cast<uint16*>(data);
        uint32* source = reinterpret_cast<uint32*>(shadow_buffer_.get());
        uint32* end = source + num_elements();
        for (; source < end; ++source, ++destination) {
          *destination = *source;
        }
        d3d_buffer_->Unlock();
        dirty_ = false;
        return true;
      }
      return false;
    }
    return true;
  } else {
    return HR(d3d_buffer_->Unlock());
  }
}

}  // namespace o3d
