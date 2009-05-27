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


// This file contains the declaration of ParamCache.

#ifndef O3D_CORE_CROSS_PARAM_CACHE_H_
#define O3D_CORE_CROSS_PARAM_CACHE_H_

#include <vector>
#include "core/cross/param_object.h"
#include "core/cross/stream_bank.h"

namespace o3d {

class Element;
class Renderer;
class Material;
class DrawElement;
class Effect;

// A ParamCache holds on to a specific set of Platform specific shader params
// to O3D param cached map to make rendering faster.
class ParamCache {
 public:
  ParamCache() : rebuild_cache_(true) {}
  virtual ~ParamCache() {}

  // Clears any internal Param to Shader Parameter cache.
  void ClearParamCache();

  // Checks if the cache of Params to Effect parameters is valid and if it is
  // not rebuilds the cache.
  // Returns true if the cache was valid, false if it was rebuilt.
  bool ValidateAndCacheParams(Effect* effect,
                              DrawElement* draw_element,
                              Element* element,
                              StreamBank* stream_bank,
                              Material* material,
                              ParamObject* override);

  // Update the cache of params.
  virtual void UpdateCache(Effect* effect,
                           DrawElement* draw_element,
                           Element* element,
                           Material* material,
                           ParamObject* override) = 0;
 protected:
  // Validates platform specific information about the effect.
  // Returns:
  //   True if effect and cache are valid.
  virtual bool ValidateEffect(Effect* effect) = 0;

 private:
  // If true we need to rebuild the cache of Params to Shader Parameters.
  bool rebuild_cache_;

  // A class to track the change of an object and its parameters.
  template <typename T, typename TPOINTER>
  class ChangeTracker {
   public:
    ChangeTracker()
        : last_object_(NULL),
          last_change_count_(0) {
    }

    inline bool NeedToUpdate(TPOINTER object) {
      return object != last_object_ ||
          (object != NULL &&
           object->change_count() != last_change_count_);
    }

    inline void Update(TPOINTER object) {
      last_object_ = T(object);
      last_change_count_ = object->change_count();
    }

   private:
    T last_object_;
    int last_change_count_;
  };

  // These fields track whether this cache matches a certain set of inputs.
  ChangeTracker<ParamObject::Ref, ParamObject*> draw_element_tracker_;
  ChangeTracker<ParamObject::Ref, ParamObject*> element_tracker_;
  ChangeTracker<ParamObject::Ref, ParamObject*> material_tracker_;
  ChangeTracker<ParamObject::Ref, ParamObject*> effect_tracker_;
  ChangeTracker<StreamBank::Ref, StreamBank*> stream_bank_tracker_;
  ChangeTracker<ParamObject*, ParamObject*> override_tracker_;

  // NOTE: override_tracker_ has to be a pointer, not a Ref because the cache is
  // stored inside a Transform and ends up referencing itself making it
  // unfreeable. Is okay that it's a pointer though because a cache is always
  // used for by a transform for itself so there is no chance that the
  // override_tracker's pointer will point to something that was freed.

  DISALLOW_COPY_AND_ASSIGN(ParamCache);
};

// A ParamCacheManager manages an array of ParamCaches.
class ParamCacheManager {
 public:
  explicit ParamCacheManager(Renderer* renderer);
  ~ParamCacheManager();

  // Returns the next ParamCache
  // Returns:
  //   The next ParamCache
  ParamCache* GetNextCache(Renderer* renderer);

 private:
  typedef std::vector<ParamCache*> ParamCacheArray;
  ParamCacheArray param_caches_;

  unsigned top_cache_index_;

  // If this does not match the current render count we need to reset
  // the top cache.
  int last_render_count_;

  DISALLOW_COPY_AND_ASSIGN(ParamCacheManager);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_PARAM_CACHE_H_
