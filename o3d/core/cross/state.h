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


// This file contains the declaration to the State class.

#ifndef O3D_CORE_CROSS_STATE_H_
#define O3D_CORE_CROSS_STATE_H_

#include "core/cross/param_object.h"
#include "core/cross/param.h"
#include "core/cross/types.h"

namespace o3d {

class Texture;
class Renderer;

// A State handles setting render states for rendering (eg. polygon offset,
// alpha). State settings are set using Params. This allows a State to set as
// few params as possible and also makes it easy to query and expand as needed.
class State : public ParamObject {
 public:
  typedef SmartPointer<State> Ref;
  typedef WeakPointer<State> WeakPointerType;

  State(ServiceLocator* service_locator,
        Renderer* renderer);

  static const char* kAlphaTestEnableParamName;
  static const char* kAlphaReferenceParamName;
  static const char* kAlphaComparisonFunctionParamName;
  static const char* kCullModeParamName;
  static const char* kDitherEnableParamName;
  static const char* kLineSmoothEnableParamName;
  static const char* kPointSpriteEnableParamName;
  static const char* kPointSizeParamName;
  static const char* kPolygonOffset1ParamName;
  static const char* kPolygonOffset2ParamName;
  static const char* kFillModeParamName;
  static const char* kZEnableParamName;
  static const char* kZWriteEnableParamName;
  static const char* kZComparisonFunctionParamName;
  static const char* kAlphaBlendEnableParamName;
  static const char* kSourceBlendFunctionParamName;
  static const char* kDestinationBlendFunctionParamName;
  static const char* kStencilEnableParamName;
  static const char* kStencilFailOperationParamName;
  static const char* kStencilZFailOperationParamName;
  static const char* kStencilPassOperationParamName;
  static const char* kStencilComparisonFunctionParamName;
  static const char* kStencilReferenceParamName;
  static const char* kStencilMaskParamName;
  static const char* kStencilWriteMaskParamName;
  static const char* kColorWriteEnableParamName;
  static const char* kBlendEquationParamName;
  static const char* kTwoSidedStencilEnableParamName;
  static const char* kCCWStencilFailOperationParamName;
  static const char* kCCWStencilZFailOperationParamName;
  static const char* kCCWStencilPassOperationParamName;
  static const char* kCCWStencilComparisonFunctionParamName;
  static const char* kSeparateAlphaBlendEnableParamName;
  static const char* kSourceBlendAlphaFunctionParamName;
  static const char* kDestinationBlendAlphaFunctionParamName;
  static const char* kBlendAlphaEquationParamName;

  // Comparison Operations Enumeration
  enum Comparison {
    CMP_NEVER,  /* Never */
    CMP_LESS,  /* Less Than */
    CMP_EQUAL,  /* Equal To */
    CMP_LEQUAL,  /* Less Than or Equal To */
    CMP_GREATER,  /* Greater Than */
    CMP_NOTEQUAL,  /* Not Equal To */
    CMP_GEQUAL,  /* Greater Than or Equal To */
    CMP_ALWAYS,   /* Always */
  };

  // Culling Operations Enumeration
  enum Cull {
    CULL_NONE,  /* Don't cull */
    CULL_CW,  /* Cull clock-wise faces*/
    CULL_CCW,  /* Cull counter clock-wise */
  };

  // Fill Mode Enumeration
  enum Fill {
    POINT,  /* Points only */
    WIREFRAME,  /* Wireframe */
    SOLID,  /* Solid fill */
  };

  // Blending Functions
  enum BlendingFunction {
    BLENDFUNC_ZERO,
    BLENDFUNC_ONE,
    BLENDFUNC_SOURCE_COLOR,
    BLENDFUNC_INVERSE_SOURCE_COLOR,
    BLENDFUNC_SOURCE_ALPHA,
    BLENDFUNC_INVERSE_SOURCE_ALPHA,
    BLENDFUNC_DESTINATION_ALPHA,
    BLENDFUNC_INVERSE_DESTINATION_ALPHA,
    BLENDFUNC_DESTINATION_COLOR,
    BLENDFUNC_INVERSE_DESTINATION_COLOR,
    BLENDFUNC_SOURCE_ALPHA_SATUTRATE,
  };

  enum BlendingEquation {
    BLEND_ADD,
    BLEND_SUBTRACT,
    BLEND_REVERSE_SUBTRACT,
    BLEND_MIN,
    BLEND_MAX,
  };

  enum StencilOperation {
    STENCIL_KEEP,
    STENCIL_ZERO,
    STENCIL_REPLACE,
    STENCIL_INCREMENT_SATURATE,
    STENCIL_DECREMENT_SATURATE,
    STENCIL_INVERT,
    STENCIL_INCREMENT,
    STENCIL_DECREMENT,
  };

  // Returns a Param for a given state. If the param does not already exist it
  // will be created. If the state_name is invalid it will return NULL
  // Parameters:
  //   state_name: name of the state
  // Returns:
  //   Pointer to param or NULL if no matching state.
  Param* GetUntypedStateParam(const String& state_name);

  // Returns a Param for a given state. If the param does not already exist it
  // will be created. If the state_name is invalid it will return NULL.  If
  // the template parameter is incorrect for the given state, it will return
  // NULL
  // Parameters:
  //   T:  the type of the state
  //   state_name: name of the state
  // Returns:
  //   Pointer to param or NULL if no matching state.
  template<class T>
  T* GetStateParam(const String& state_name) {
    return ObjectBase::rtti_dynamic_cast<T>(GetUntypedStateParam(state_name));
  }

  // Overridden from ParamObject. We don't allow mis-matched params
  // to get created.
  virtual bool OnBeforeAddParam(Param* param);

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  Renderer* renderer_;

  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  O3D_DECL_CLASS(State, NamedObject);
  DISALLOW_COPY_AND_ASSIGN(State);
};  // State

class ParamState : public TypedRefParam<State> {
 public:
  typedef SmartPointer<ParamState> Ref;

  ParamState(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedRefParam<State>(service_locator, dynamic, read_only) {}

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamState, RefParamBase)
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_STATE_H_
