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


// This file contains the definition of the ParamCacheD3D9 class.

#include "core/cross/precompile.h"
#include "core/win/d3d9/param_cache_d3d9.h"
#include "core/win/d3d9/effect_d3d9.h"
#include "core/cross/element.h"
#include "core/cross/draw_element.h"

namespace o3d {

ParamCacheD3D9::ParamCacheD3D9(ServiceLocator* service_locator)
    : semantic_manager_(service_locator->GetService<SemanticManager>()),
      last_vertex_shader_(NULL),
      last_fragment_shader_(NULL) {
}

bool ParamCacheD3D9::ValidateEffect(Effect* effect) {
  DLOG_ASSERT(effect);

  EffectD3D9* effect_d3d9 = down_cast<EffectD3D9*>(effect);
  IDirect3DVertexShader9* vertex_shader = effect_d3d9->d3d_vertex_shader();
  IDirect3DPixelShader9* fragment_shader = effect_d3d9->d3d_fragment_shader();

  return (last_fragment_shader_ == fragment_shader ||
          last_vertex_shader_ == vertex_shader);
}

void ParamCacheD3D9::UpdateCache(Effect* effect,
                                 DrawElement* draw_element,
                                 Element* element,
                                 Material* material,
                                 ParamObject* override) {
  DLOG_ASSERT(effect);
  EffectD3D9* effect_d3d9 = down_cast<EffectD3D9*>(effect);

  std::vector<ParamObject*> param_object_list;
  param_object_list.push_back(override);
  param_object_list.push_back(draw_element);
  param_object_list.push_back(element);
  param_object_list.push_back(material);
  param_object_list.push_back(effect);
  param_object_list.push_back(semantic_manager_->sas_param_object());

  effect_d3d9->UpdateParameterMappings(param_object_list,
                                       &cached_effect_params_);

  last_vertex_shader_ = effect_d3d9->d3d_vertex_shader();
  last_fragment_shader_ = effect_d3d9->d3d_fragment_shader();}

}  // namespace o3d

