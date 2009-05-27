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


// This file contains the declaration of the ParamCacheGL class.

#ifndef O3D_CORE_CROSS_GL_PARAM_CACHE_GL_H_
#define O3D_CORE_CROSS_GL_PARAM_CACHE_GL_H_

#include <map>
#include "core/cross/param_cache.h"
#include "core/cross/gl/effect_gl.h"

namespace o3d {

class ParamTexture;
class SemanticManager;

class ParamCacheGL : public ParamCache {
 public:
  ParamCacheGL(SemanticManager* semantic_manager, Renderer* renderer);

  typedef std::map<CGparameter, int> VaryingParameterMap;
  typedef std::map<CGparameter, EffectParamHandlerGL::Ref> UniformParameterMap;
  typedef std::map<CGparameter, ParamTexture*> SamplerParameterMap;

  // Overridden from ParamCache.
  virtual void UpdateCache(Effect* effect,
                           DrawElement* draw_element,
                           Element* element,
                           Material* material,
                           ParamObject* override);

  VaryingParameterMap& varying_map() { return varying_map_; }
  UniformParameterMap& uniform_map() { return uniform_map_; }
  SamplerParameterMap& sampler_map() { return sampler_map_; }

 protected:
  // Overridden from ParamCache
  // Validates platform specific information about the effect.
  virtual bool ValidateEffect(Effect* effect);

 private:

  SemanticManager* semantic_manager_;
  Renderer* renderer_;

  // Records the last two shaders used on this cache, allowing us to rescan the
  // shader parameters if the user changes the shader on an active cache.
  CGprogram last_vertex_program_;
  CGprogram last_fragment_program_;

  // Search the leaf parameters of a CGeffect and it's shaders for
  // parameters using cgGetFirstEffectParameter() /
  // cgGetFirstLeafParameter() / cgGetNextLeafParameter(). Add the
  // CGparameters found to the parameter maps on the DrawElement.
  void ScanCgEffectParameters(CGprogram cg_vertex,
                              CGprogram fragment,
                              ParamObject* draw_element,
                              ParamObject* element,
                              Material* material,
                              ParamObject* override);

  // A map of varying CGparameter to Stream index.
  VaryingParameterMap varying_map_;
  // A map of uniform CGparameter to Param objects.
  UniformParameterMap uniform_map_;
  // A map of uniform CG_SAMPLER CGparameters to ParamTexture objects.
  // TODO: remove this (OLD path for textures).
  SamplerParameterMap sampler_map_;
};
}  // o3d

#endif  // O3D_CORE_CROSS_GL_PARAM_CACHE_GL_H_
