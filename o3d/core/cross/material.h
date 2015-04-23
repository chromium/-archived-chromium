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


// This file contains the Material declaration.

#ifndef O3D_CORE_CROSS_MATERIAL_H_
#define O3D_CORE_CROSS_MATERIAL_H_

#include "core/cross/param_object.h"
#include "core/cross/state.h"
#include "core/cross/effect.h"
#include "core/cross/draw_list.h"

namespace o3d {

// A Material represents an Effect with a specific set of parameters. For
// example a Lambert effect with a "diffuseColor" set to blue vs a Lambert
// effect with "diffuseColor" set to red. Note that a material MUST have its
// draw_list set in order for objects using it to render.
class Material : public ParamObject {
 public:
  typedef SmartPointer<Material> Ref;
  typedef WeakPointer<Material> WeakPointerType;

  // Names of Material Params.
  static const char* kEffectParamName;
  static const char* kStateParamName;
  static const char* kDrawListParamName;

  // Returns the Effect object bound to the Material.
  Effect* effect() const {
    return effect_param_ref_->value();
  }

  // Binds an Effect object to the Material.
  void set_effect(Effect *effect) {
    effect_param_ref_->set_value(effect);
  }

  // Returns the State object bound to the Material.
  State* state() const {
    return state_param_ref_->value();
  }

  // Binds a State object to the Material.
  void set_state(State *state) {
    state_param_ref_->set_value(state);
  }

  // Gets the pass list.
  DrawList* draw_list() const {
    return draw_list_param_->value();
  }

  // Sets the pass list.
  void set_draw_list(DrawList* value) {
    draw_list_param_->set_value(value);
  }

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

 private:
  explicit Material(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  ParamState::Ref state_param_ref_;  // State for material.
  ParamEffect::Ref effect_param_ref_;  // Effect for material.
  ParamDrawList::Ref draw_list_param_;  // DrawList we will go on.

  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  O3D_DECL_CLASS(Material, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(Material);
};

class ParamMaterial : public TypedRefParam<Material> {
 public:
  typedef SmartPointer<ParamMaterial> Ref;

  ParamMaterial(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedRefParam<Material>(service_locator, dynamic, read_only) {
  }

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamMaterial, RefParamBase)
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_MATERIAL_H_
