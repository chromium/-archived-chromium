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


// This file contains the definition of DrawElement.

#include "core/cross/precompile.h"
#include "core/cross/param_cache.h"
#include "core/cross/effect.h"
#include "core/cross/element.h"
#include "core/cross/draw_element.h"
#include "core/cross/material.h"
#include "core/cross/renderer.h"

namespace o3d {

void ParamCache::ClearParamCache() {
  rebuild_cache_ = true;
}

bool ParamCache::ValidateAndCacheParams(Effect* effect,
                                        DrawElement* draw_element,
                                        Element* element,
                                        StreamBank* stream_bank,
                                        Material* material,
                                        ParamObject* override) {
  if (rebuild_cache_ ||
      draw_element_tracker_.NeedToUpdate(draw_element) ||
      element_tracker_.NeedToUpdate(element) ||
      material_tracker_.NeedToUpdate(material) ||
      effect_tracker_.NeedToUpdate(effect) ||
      stream_bank_tracker_.NeedToUpdate(stream_bank) ||
      override_tracker_.NeedToUpdate(override) ||
      !ValidateEffect(effect)) {
    UpdateCache(effect, draw_element, element, material, override);
    draw_element_tracker_.Update(draw_element);
    element_tracker_.Update(element);
    material_tracker_.Update(material);
    effect_tracker_.Update(effect);
    stream_bank_tracker_.Update(stream_bank);
    override_tracker_.Update(override);
    rebuild_cache_ = false;
    return false;
  }
  return true;
}

ParamCacheManager::ParamCacheManager(Renderer* renderer)
    : top_cache_index_(0),
      last_render_count_(renderer->render_frame_count() - 1) {
}

ParamCacheManager::~ParamCacheManager() {
  for (unsigned ii = 0; ii < param_caches_.size(); ++ii) {
    delete param_caches_[ii];
  }
}

ParamCache* ParamCacheManager::GetNextCache(Renderer* renderer) {
  // If this is the first time we've been asked for a cache this frame
  // reset to start using the first cache.
  if (renderer->render_frame_count() != last_render_count_) {
    last_render_count_ = renderer->render_frame_count();
    // remove any un-needed caches.
    for (unsigned ii = top_cache_index_; ii < param_caches_.size(); ++ii) {
      renderer->FreeParamCache(param_caches_[ii]);
      param_caches_[ii] = NULL;
    }

    // Reset to first cache.
    top_cache_index_ = 0;
  }

  // ParamCaches only get created once and then reused. This saves lots of
  // allocations/deallocation that would otherwise happen every frame.

  // If I instead used size() then I would have to clear() param_caches_ each
  // frame which in turn would mean I'd need to keep a separate container of
  // ParamCaches to keep track of the ParamCaches I'm keeping around and pulling
  // them out of that container and putting them in this one.

  // Instead I just keep using the ones in this container and use
  // top_cache_index_ to track the highest used ParamCache.
  if (top_cache_index_ >= param_caches_.size()) {
    param_caches_.push_back(NULL);
  }
  ParamCache* param_cache = param_caches_[top_cache_index_];
  if (!param_cache) {
    param_cache = renderer->CreateParamCache();
    param_caches_[top_cache_index_] = param_cache;
  }
  ++top_cache_index_;
  return param_cache;
}

}  // namespace o3d





