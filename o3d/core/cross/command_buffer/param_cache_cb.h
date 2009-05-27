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


// This file contains the declaration of the ParamCacheCB class.

#ifndef O3D_CORE_CROSS_COMMAND_BUFFER_PARAM_CACHE_CB_H_
#define O3D_CORE_CROSS_COMMAND_BUFFER_PARAM_CACHE_CB_H_

#include <vector>
#include "core/cross/param_cache.h"
#include "core/cross/command_buffer/effect_cb.h"

namespace o3d {

class ParamHandlerCB;

// This class is the command-buffer implementation of the ParamCache class. It
// keeps a list of Param * -> param ResourceID associations handlers.
class ParamCacheCB : public ParamCache {
 public:
  ParamCacheCB();
  virtual ~ParamCacheCB();

  // Updates the cache of params, re-creating the list of handlers.
  // Overridden from ParamCache.
  virtual void UpdateCache(Effect* effect,
                           DrawElement* draw_element,
                           Element* element,
                           Material* material,
                           ParamObject* override);

  // Runs all the cached handlers.
  void RunHandlers(command_buffer::CommandBufferHelper *helper);

 protected:
  // Validates platform specific information about the effect.
  // Overridden from ParamCache.
  virtual bool ValidateEffect(Effect* effect);

 private:
  // Destroys the handlers and clears the list.
  void ClearHandlers();

  unsigned int last_effect_generation_;
  Effect::MatrixLoadOrder last_matrix_load_order_;
  std::vector<ParamHandlerCB *> handlers_;
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_COMMAND_BUFFER_PARAM_CACHE_CB_H_
