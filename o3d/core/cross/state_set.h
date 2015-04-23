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


// This file contains the StateSet render node declaration.

#ifndef O3D_CORE_CROSS_STATE_SET_H_
#define O3D_CORE_CROSS_STATE_SET_H_

#include "core/cross/render_node.h"
#include "core/cross/state.h"

namespace o3d {

// A StateSet is a render node that sets render states for all of its children.
class StateSet : public RenderNode {
 public:
  typedef SmartPointer<StateSet> Ref;

  // Names of StateSet Params.
  static const char* kStateParamName;

  // Gets the state.
  State* state() const {
    return state_param_->value();
  }

  // Sets the state.
  void set_state(State* value) {
    state_param_->set_value(value);
  }

  // Overridden from RenderNode. Sets the state.
  virtual void Render(RenderContext* render_context);

  // Overridden from RenderNode. Restores the state.
  virtual void PostRender(RenderContext* render_context);

 private:
  explicit StateSet(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  ParamState::Ref state_param_;  // state

  O3D_DECL_CLASS(StateSet, RenderNode);
  DISALLOW_COPY_AND_ASSIGN(StateSet);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_STATE_SET_H_




