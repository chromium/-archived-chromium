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


// This file contains the definition of the PrimitiveCB class.

#ifndef O3D_CORE_CROSS_COMMAND_BUFFER_STREAM_BANK_CB_H_
#define O3D_CORE_CROSS_COMMAND_BUFFER_STREAM_BANK_CB_H_

#include "core/cross/precompile.h"
#include "core/cross/stream_bank.h"
#include "command_buffer/common/cross/resource.h"

namespace o3d {

class RendererCB;

// StreamBankCB is the command-buffer implementation of the StreamBank. It
// provides the necessary interfaces for setting the geometry streams on the
// StreamBank.
class StreamBankCB : public StreamBank {
 public:
  StreamBankCB(ServiceLocator* service_locator, RendererCB *renderer);
  virtual ~StreamBankCB();

  // Creates the vertex struct from the vertex streams.
  void CreateVertexStruct();

  // Binds the streams for rendering.
  void BindStreamsForRendering();

 protected:
  // Overridden from StreamBank.
  virtual void OnUpdateStreams();

 private:
  // Destroys the vertex struct.
  void DestroyVertexStruct();

  // The renderer that created this shape data.
  RendererCB *renderer_;

  // The resource ID for the vertex struct representing the input vertex
  // streams, or kInvalidResource if it hasn't been created yet.
  command_buffer::ResourceID vertex_struct_id_;
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_COMMAND_BUFFER_STREAM_BANK_CB_H_
