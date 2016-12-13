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


// This file contains the declaration of the ParamCacheD3D9 class.

#ifndef O3D_CORE_WIN_D3D9_PARAM_CACHE_D3D9_H_
#define O3D_CORE_WIN_D3D9_PARAM_CACHE_D3D9_H_

#include <d3d9.h>
#include <atlbase.h>
#include "core/cross/param_cache.h"
#include "core/win/d3d9/effect_d3d9.h"
#include "core/cross/semantic_manager.h"

namespace o3d {

class ParamCacheD3D9 : public ParamCache {
 public:
  explicit ParamCacheD3D9(ServiceLocator* service_locator);

  // Overridden from ParamCache.
  virtual void UpdateCache(Effect* effect,
                           DrawElement* draw_element,
                           Element* element,
                           Material* material,
                           ParamObject* override);

  const EffectParamHandlerCacheD3D9& cached_effect_params() {
    return cached_effect_params_;
  };
 protected:
  // Overridden from ParamCache
  // Validates platform specific information about the effect.
  virtual bool ValidateEffect(Effect* effect);

 private:
  EffectParamHandlerCacheD3D9 cached_effect_params_;
  SemanticManager* semantic_manager_;

  CComPtr<IDirect3DVertexShader9> last_vertex_shader_;
  CComPtr<IDirect3DPixelShader9> last_fragment_shader_;
};
}  // o3d

#endif  // O3D_CORE_WIN_D3D9_PARAM_CACHE_D3D9_H_
