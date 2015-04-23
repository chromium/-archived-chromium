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


// This file contains the declaration of the command-buffer version of the
// sampler.

#ifndef O3D_CORE_CROSS_COMMAND_BUFFER_SAMPLER_CB_H_
#define O3D_CORE_CROSS_COMMAND_BUFFER_SAMPLER_CB_H_

#include "core/cross/precompile.h"
#include "core/cross/sampler.h"
#include "command_buffer/common/cross/resource.h"

namespace o3d {

class RendererCB;

// SamplerCB is an implementation of the Sampler object for command buffers.
class SamplerCB : public Sampler {
 public:
  SamplerCB(ServiceLocator* service_locator, RendererCB* renderer);
  virtual ~SamplerCB();

  // Sets the d3d texture and sampler states for the given sampler unit.
  void SetTextureAndStates();

  // Gets the resource ID for this sampler.
  command_buffer::ResourceID resource_id() const { return resource_id_; }

 private:
  RendererCB* renderer_;
  command_buffer::ResourceID resource_id_;
  DISALLOW_COPY_AND_ASSIGN(SamplerCB);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_COMMAND_BUFFER_SAMPLER_CB_H_
