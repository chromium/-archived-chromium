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


// This file defines the command-buffer version of the Effect class.

#ifndef O3D_CORE_CROSS_COMMAND_BUFFER_EFFECT_CB_H_
#define O3D_CORE_CROSS_COMMAND_BUFFER_EFFECT_CB_H_

#include <vector>
#include "core/cross/effect.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "command_buffer/client/cross/effect_helper.h"

namespace o3d {

class ParamCacheCB;

// This class is the command-buffer implementation of the Effect class.
class EffectCB : public Effect {
 public:
  friend class ParamCacheCB;
  typedef command_buffer::EffectHelper EffectHelper;

  EffectCB(ServiceLocator *service_locator, RendererCB *renderer);
  virtual ~EffectCB();

  // Loads the vertex and fragment shader programs from a string containing
  // a DirectX FX description.
  virtual bool LoadFromFXString(const String& effect);

  // Gets the ResourceID of the effect.
  command_buffer::ResourceID resource_id() { return resource_id_; }

 protected:
  // Gets info about the parameters this effect needs.
  // Overriden from Effect.
  virtual void GetParameterInfo(EffectParameterInfoArray* info_array);

  // Gets info about the varying parameters this effects vertex shader needs.
  // Parameters:
  //   info_array: EffectStreamInfoArray to receive info.
  virtual void GetStreamInfo(EffectStreamInfoArray* info_array);

 private:
  void Destroy();
  // The command buffer resource ID for the effect.
  command_buffer::ResourceID resource_id_;
  std::vector<EffectHelper::EffectParamDesc> param_descs_;
  std::vector<EffectHelper::EffectStreamDesc> stream_descs_;
  // A generation counter to dirty ParamCacheCBs.
  unsigned int generation_;
  // The renderer that created this effect.
  RendererCB *renderer_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(EffectCB);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_COMMAND_BUFFER_EFFECT_CB_H_
