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


// This file contains the declaration of the EffectGL class.

#ifndef O3D_CONVERTER_CROSS_EFFECT_STUB_H_
#define O3D_CONVERTER_CROSS_EFFECT_STUB_H_

#include "core/cross/effect.h"

namespace o3d {

class ServiceLocator;

// EffectStub is an implementation of the Effect object for the
// converter.  It provides the API for setting the vertex and fragment
// shaders for the Effect using the Cg Runtime.
class EffectStub : public Effect {
 public:
  explicit EffectStub(ServiceLocator* service_locator)
      : Effect(service_locator) {}
  virtual ~EffectStub() {}

  // Reads the vertex and fragment shaders from string in the FX format.
  // It returns true if the shaders were successfully compiled.
  virtual bool LoadFromFXString(const String& effect) { return true; }

  // Gets info about the parameters this effect needs.
  // Overriden from Effect.
  virtual void GetParameterInfo(EffectParameterInfoArray* info_array) {}

  // Gets info about the varying parameters this effects vertex shader needs.
  // Parameters:
  //   info_array: EffectParameterInfoArray to receive info.
  virtual void GetStreamInfo(
      EffectStreamInfoArray* info_array) {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EffectStub);
};
}  // namespace o3d

#endif  // O3D_CONVERTER_CROSS_EFFECT_STUB_H_
