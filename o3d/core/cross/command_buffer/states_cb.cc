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


// This file contains the implementation of the RendererCB::StateManager class,
// including all the state handlers for the command buffer renderer.

#include "core/cross/precompile.h"
#include "core/cross/state.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "core/cross/command_buffer/states_cb.h"
#include "command_buffer/common/cross/gapi_interface.h"

namespace o3d {
using command_buffer::CommandBufferEntry;
using command_buffer::CommandBufferHelper;
using command_buffer::GAPIInterface;

namespace {

// Converts values meant to represent a Cull Mode to the corresponding
// command-buffer value.
// Default: CULL_NONE.
GAPIInterface::FaceCullMode CullModeToCB(int cull) {
  switch (cull) {
    default:
    case State::CULL_NONE:
      return GAPIInterface::CULL_NONE;
    case State::CULL_CW:
      return GAPIInterface::CULL_CW;
    case State::CULL_CCW:
      return GAPIInterface::CULL_CCW;
  }
}

// Converts values meant to represent a Polygon Fill Mode to the corresponding
// command-buffer value.
// Default: POLYGON_MODE_FILL.
GAPIInterface::PolygonMode FillModeToCB(int fill) {
  switch (fill) {
    case State::POINT:
      return GAPIInterface::POLYGON_MODE_POINTS;
    case State::WIREFRAME:
      return GAPIInterface::POLYGON_MODE_LINES;
    default:
    case State::SOLID:
      return GAPIInterface::POLYGON_MODE_FILL;
  }
}

// Converts values meant to represent a Comparison Function to the corresponding
// command-buffer value.
// Default: ALWAYS.
GAPIInterface::Comparison ComparisonToCB(int comparison) {
  switch (comparison) {
    case State::CMP_NEVER:
      return GAPIInterface::NEVER;
    case State::CMP_LESS:
      return GAPIInterface::LESS;
    case State::CMP_EQUAL:
      return GAPIInterface::EQUAL;
    case State::CMP_LEQUAL:
      return GAPIInterface::LEQUAL;
    case State::CMP_GREATER:
      return GAPIInterface::GREATER;
    case State::CMP_NOTEQUAL:
      return GAPIInterface::NOT_EQUAL;
    case State::CMP_GEQUAL:
      return GAPIInterface::GEQUAL;
    case State::CMP_ALWAYS:
    default:
      return GAPIInterface::ALWAYS;
  }
}

// Converts values meant to represent a Stencil Operation to the corresponding
// command-buffer value.
// Default: KEEP.
GAPIInterface::StencilOp StencilOpToCB(int op) {
  switch (op) {
    default:
    case State::STENCIL_KEEP:
      return GAPIInterface::KEEP;
    case State::STENCIL_ZERO:
      return GAPIInterface::ZERO;
    case State::STENCIL_REPLACE:
      return GAPIInterface::REPLACE;
    case State::STENCIL_INCREMENT_SATURATE:
      return GAPIInterface::INC_NO_WRAP;
    case State::STENCIL_DECREMENT_SATURATE:
      return GAPIInterface::DEC_NO_WRAP;
    case State::STENCIL_INVERT:
      return GAPIInterface::INVERT;
    case State::STENCIL_INCREMENT:
      return GAPIInterface::INC_WRAP;
    case State::STENCIL_DECREMENT:
      return GAPIInterface::DEC_WRAP;
  }
}

// Converts values meant to represent a Blending Function to the corresponding
// command-buffer value.
// Default: BLEND_FUNC_ONE.
GAPIInterface::BlendFunc BlendFuncToCB(int func) {
  switch (func) {
    case State::BLENDFUNC_ZERO:
      return GAPIInterface::BLEND_FUNC_ZERO;
    default:
    case State::BLENDFUNC_ONE:
      return GAPIInterface::BLEND_FUNC_ONE;
    case State::BLENDFUNC_SOURCE_COLOR:
      return GAPIInterface::BLEND_FUNC_SRC_COLOR;
    case State::BLENDFUNC_INVERSE_SOURCE_COLOR:
      return GAPIInterface::BLEND_FUNC_INV_SRC_COLOR;
    case State::BLENDFUNC_SOURCE_ALPHA:
      return GAPIInterface::BLEND_FUNC_SRC_ALPHA;
    case State::BLENDFUNC_INVERSE_SOURCE_ALPHA:
      return GAPIInterface::BLEND_FUNC_INV_SRC_ALPHA;
    case State::BLENDFUNC_DESTINATION_ALPHA:
      return GAPIInterface::BLEND_FUNC_DST_ALPHA;
    case State::BLENDFUNC_INVERSE_DESTINATION_ALPHA:
      return GAPIInterface::BLEND_FUNC_INV_DST_ALPHA;
    case State::BLENDFUNC_DESTINATION_COLOR:
      return GAPIInterface::BLEND_FUNC_DST_COLOR;
    case State::BLENDFUNC_INVERSE_DESTINATION_COLOR:
      return GAPIInterface::BLEND_FUNC_INV_DST_COLOR;
    case State::BLENDFUNC_SOURCE_ALPHA_SATUTRATE:
      return GAPIInterface::BLEND_FUNC_SRC_ALPHA_SATUTRATE;
  }
}

// Converts values meant to represent a Blending Equation to the corresponding
// command-buffer value.
// Default: BLEND_EQ_ADD.
GAPIInterface::BlendEq BlendEqToCB(int eq) {
  switch (eq) {
    default:
    case State::BLEND_ADD:
      return GAPIInterface::BLEND_EQ_ADD;
    case State::BLEND_SUBTRACT:
      return GAPIInterface::BLEND_EQ_SUB;
    case State::BLEND_REVERSE_SUBTRACT:
      return GAPIInterface::BLEND_EQ_REV_SUB;
    case State::BLEND_MIN:
      return GAPIInterface::BLEND_EQ_MIN;
    case State::BLEND_MAX:
      return GAPIInterface::BLEND_EQ_MAX;
  }
}

}  // anonymous namespace

// This class wraps StateHandler to make it type-safe.
template <typename T>
class TypedStateHandler : public RendererCB::StateHandler {
 public:
  // Override this function to set a specific state.
  // Parameters:
  //   renderer: The platform specific renderer.
  //   param: A concrete param with state data.
  virtual void SetStateFromTypedParam(RendererCB* renderer, T* param) const = 0;

  // Gets Class of State's Parameter
  virtual const ObjectBase::Class* GetClass() const {
    return T::GetApparentClass();
  }

 private:
  // Calls SetStateFromTypedParam if the Param type is the correct type.
  // Parameters:
  //   renderer: The renderer.
  //   param: A param with state data.
  virtual void SetState(Renderer* renderer, Param* param) const {
    RendererCB *renderer_cb = down_cast<RendererCB *>(renderer);
    // This is safe because State guarntees Params match by type.
    DCHECK(param->IsA(T::GetApparentClass()));
    SetStateFromTypedParam(renderer_cb, down_cast<T*>(param));
  }
};

// A template that generates a handler for enable/disable states.
// Template Parameters:
//   bit_field: the BitField type to access the proper bit in the
//   command-buffer argument value.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
template <typename bit_field>
class EnableStateHandler : public TypedStateHandler<ParamBoolean> {
 public:
  EnableStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamBoolean* param) const {
    bit_field::Set(value_, param->value() ? 1 : 0);
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// A template that generates a handler for bit field states.
// Template Parameters:
//   bit_field: the BitField type to access the proper bit field.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
template <typename bit_field>
class BitFieldStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  BitFieldStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    bit_field::Set(value_, param->value());
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// A template that generates a handler for full-size values (uint32, int32,
// float).
// Template Parameters:
//   ParamType: the type of the parameter.
// Parameters:
//   value: a pointer to the argument value as the proper type.
//   dirty: a pointer to the dirty bit.
template <typename ParamType>
class ValueStateHandler : public TypedStateHandler<ParamType> {
 public:
  typedef typename ParamType::DataType DataType;
  ValueStateHandler(DataType *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamType* param) const {
    *value_ = param->value();
    *dirty_ = true;
  }
 private:
  DataType *value_;
  bool *dirty_;
};

// Handler for Cull Mode.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
class CullModeStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  CullModeStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    using command_buffer::set_polygon_raster::CullMode;
    CullMode::Set(value_, CullModeToCB(param->value()));
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// Handler for Polygon Fill Mode.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
class FillModeStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  FillModeStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    using command_buffer::set_polygon_raster::FillMode;
    FillMode::Set(value_, FillModeToCB(param->value()));
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// A template that generates a handler for comparison functions.
// Template Parameters:
//   bit_field: the BitField type to access the proper bits.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
template <typename bit_field>
class ComparisonStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  ComparisonStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    bit_field::Set(value_, ComparisonToCB(param->value()));
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// A template that generates a handler for stencil operations.
// Template Parameters:
//   bit_field: the BitField type to access the proper bits.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
template <typename bit_field>
class StencilOpStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  StencilOpStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    bit_field::Set(value_, StencilOpToCB(param->value()));
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// A template that generates a handler for blend equations.
// Template Parameters:
//   bit_field: the BitField type to access the proper bits.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
template <typename bit_field>
class BlendFuncStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  BlendFuncStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    bit_field::Set(value_, BlendFuncToCB(param->value()));
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// A template that generates a handler for blend equations.
// Template Parameters:
//   bit_field: the BitField type to access the proper bits.
// Parameters:
//   value: a pointer to the argument value as an uint32.
//   dirty: a pointer to the dirty bit.
template <typename bit_field>
class BlendEqStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  BlendEqStateHandler(uint32 *value, bool *dirty)
      : value_(value),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamInteger* param) const {
    bit_field::Set(value_, BlendEqToCB(param->value()));
    *dirty_ = true;
  }
 private:
  uint32 *value_;
  bool *dirty_;
};

// A handler that sets the blending color.
// Parameters:
//   args: a pointer to the arguments.
//   dirty: a pointer to the dirty bit.
class BlendColorStateHandler : public TypedStateHandler<ParamFloat4> {
 public:
  BlendColorStateHandler(CommandBufferEntry *args, bool *dirty)
      : args_(args),
        dirty_(dirty) {
  }

  virtual void SetStateFromTypedParam(RendererCB* renderer,
                                      ParamFloat4* param) const {
    Float4 value = param->value();
    args_[0].value_float = value[0];
    args_[1].value_float = value[1];
    args_[2].value_float = value[2];
    args_[3].value_float = value[3];
    *dirty_ = true;
  }
 private:
  CommandBufferEntry *args_;
  bool *dirty_;
};

// Adds all the state handlers for all the states. The list of handlers must
// match in names and types the list in Renderer::AddDefaultStates()
// (in renderer.cc).
void RendererCB::StateManager::AddStateHandlers(RendererCB *renderer) {
  // Point/Line raster
  {
    using command_buffer::set_point_line_raster::LineSmoothEnable;
    using command_buffer::set_point_line_raster::PointSpriteEnable;
    bool *dirty = point_line_helper_.dirty_ptr();
    uint32 *arg0 = &(point_line_helper_.args()[0].value_uint32);
    float *arg1 = &(point_line_helper_.args()[1].value_float);
    renderer->AddStateHandler(
        State::kLineSmoothEnableParamName,
        new EnableStateHandler<LineSmoothEnable>(arg0, dirty));
    renderer->AddStateHandler(
        State::kPointSpriteEnableParamName,
        new EnableStateHandler<PointSpriteEnable>(arg0, dirty));
    renderer->AddStateHandler(State::kPointSizeParamName,
                              new ValueStateHandler<ParamFloat>(arg1, dirty));
  }

  // Polygon Raster
  {
    bool *dirty = poly_raster_helper_.dirty_ptr();
    uint32 *arg = &(poly_raster_helper_.args()[0].value_uint32);
    renderer->AddStateHandler(State::kCullModeParamName,
                              new CullModeStateHandler(arg, dirty));
    renderer->AddStateHandler(State::kFillModeParamName,
                              new FillModeStateHandler(arg, dirty));
  }

  // Polygon Offset
  {
    bool *dirty = poly_offset_helper_.dirty_ptr();
    float *arg0 = &(poly_offset_helper_.args()[0].value_float);
    float *arg1 = &(poly_offset_helper_.args()[1].value_float);
    renderer->AddStateHandler(State::kPolygonOffset1ParamName,
                              new ValueStateHandler<ParamFloat>(arg0, dirty));
    renderer->AddStateHandler(State::kPolygonOffset2ParamName,
                              new ValueStateHandler<ParamFloat>(arg1, dirty));
  }

  // Alpha test
  {
    using command_buffer::set_alpha_test::Enable;
    using command_buffer::set_alpha_test::Func;
    bool *dirty = alpha_test_helper_.dirty_ptr();
    uint32 *arg0 = &(alpha_test_helper_.args()[0].value_uint32);
    float *arg1 = &(alpha_test_helper_.args()[1].value_float);
    renderer->AddStateHandler(State::kAlphaTestEnableParamName,
                              new EnableStateHandler<Enable>(arg0, dirty));
    renderer->AddStateHandler(State::kAlphaComparisonFunctionParamName,
                              new ComparisonStateHandler<Func>(arg0, dirty));
    renderer->AddStateHandler(State::kAlphaReferenceParamName,
                              new ValueStateHandler<ParamFloat>(arg1, dirty));
  }

  // Depth Test
  {
    using command_buffer::set_depth_test::Enable;
    using command_buffer::set_depth_test::WriteEnable;
    using command_buffer::set_depth_test::Func;
    bool *dirty = depth_test_helper_.dirty_ptr();
    uint32 *arg = &(depth_test_helper_.args()[0].value_uint32);
    renderer->AddStateHandler(State::kZWriteEnableParamName,
                              new EnableStateHandler<WriteEnable>(arg, dirty));
    renderer->AddStateHandler(State::kZEnableParamName,
                              new EnableStateHandler<Enable>(arg, dirty));
    renderer->AddStateHandler(State::kZComparisonFunctionParamName,
                              new ComparisonStateHandler<Func>(arg, dirty));
  }

  // Stencil Test
  {
    using command_buffer::set_stencil_test::Enable;
    using command_buffer::set_stencil_test::SeparateCCW;
    using command_buffer::set_stencil_test::WriteMask;
    using command_buffer::set_stencil_test::CompareMask;
    using command_buffer::set_stencil_test::ReferenceValue;
    using command_buffer::set_stencil_test::CWFunc;
    using command_buffer::set_stencil_test::CWPassOp;
    using command_buffer::set_stencil_test::CWFailOp;
    using command_buffer::set_stencil_test::CWZFailOp;
    using command_buffer::set_stencil_test::CCWFunc;
    using command_buffer::set_stencil_test::CCWPassOp;
    using command_buffer::set_stencil_test::CCWFailOp;
    using command_buffer::set_stencil_test::CCWZFailOp;
    bool *dirty = stencil_test_helper_.dirty_ptr();
    uint32 *arg0 = &(stencil_test_helper_.args()[0].value_uint32);
    uint32 *arg1 = &(stencil_test_helper_.args()[1].value_uint32);
    renderer->AddStateHandler(State::kStencilEnableParamName,
                              new EnableStateHandler<Enable>(arg0, dirty));
    renderer->AddStateHandler(State::kTwoSidedStencilEnableParamName,
                              new EnableStateHandler<SeparateCCW>(arg0, dirty));
    renderer->AddStateHandler(
        State::kStencilReferenceParamName,
        new BitFieldStateHandler<ReferenceValue>(arg0, dirty));
    renderer->AddStateHandler(
        State::kStencilMaskParamName,
        new BitFieldStateHandler<CompareMask>(arg0, dirty));
    renderer->AddStateHandler(State::kStencilWriteMaskParamName,
                              new BitFieldStateHandler<WriteMask>(arg0, dirty));

    renderer->AddStateHandler(State::kStencilComparisonFunctionParamName,
                              new ComparisonStateHandler<CWFunc>(arg1, dirty));
    renderer->AddStateHandler(State::kStencilPassOperationParamName,
                              new StencilOpStateHandler<CWPassOp>(arg1, dirty));
    renderer->AddStateHandler(State::kStencilFailOperationParamName,
                              new StencilOpStateHandler<CWFailOp>(arg1, dirty));
    renderer->AddStateHandler(
        State::kStencilZFailOperationParamName,
        new StencilOpStateHandler<CWZFailOp>(arg1, dirty));

    renderer->AddStateHandler(State::kCCWStencilComparisonFunctionParamName,
                              new ComparisonStateHandler<CCWFunc>(arg1, dirty));
    renderer->AddStateHandler(
        State::kCCWStencilPassOperationParamName,
        new StencilOpStateHandler<CCWPassOp>(arg1, dirty));
    renderer->AddStateHandler(
        State::kCCWStencilFailOperationParamName,
        new StencilOpStateHandler<CCWFailOp>(arg1, dirty));
    renderer->AddStateHandler(
        State::kCCWStencilZFailOperationParamName,
        new StencilOpStateHandler<CCWZFailOp>(arg1, dirty));
  }

  // Blending
  {
    using command_buffer::set_blending::Enable;
    using command_buffer::set_blending::SeparateAlpha;
    using command_buffer::set_blending::ColorEq;
    using command_buffer::set_blending::ColorSrcFunc;
    using command_buffer::set_blending::ColorDstFunc;
    using command_buffer::set_blending::AlphaEq;
    using command_buffer::set_blending::AlphaSrcFunc;
    using command_buffer::set_blending::AlphaDstFunc;
    bool *dirty = blending_helper_.dirty_ptr();
    uint32 *arg = &(blending_helper_.args()[0].value_uint32);
    renderer->AddStateHandler(State::kAlphaBlendEnableParamName,
                              new EnableStateHandler<Enable>(arg, dirty));
    renderer->AddStateHandler(
        State::kSeparateAlphaBlendEnableParamName,
        new EnableStateHandler<SeparateAlpha>(arg, dirty));

    renderer->AddStateHandler(
        State::kSourceBlendFunctionParamName,
        new BlendFuncStateHandler<ColorSrcFunc>(arg, dirty));
    renderer->AddStateHandler(
        State::kDestinationBlendFunctionParamName,
        new BlendFuncStateHandler<ColorDstFunc>(arg, dirty));
    renderer->AddStateHandler(State::kBlendEquationParamName,
                              new BlendEqStateHandler<ColorEq>(arg, dirty));
    renderer->AddStateHandler(
        State::kSourceBlendAlphaFunctionParamName,
        new BlendFuncStateHandler<AlphaSrcFunc>(arg, dirty));
    renderer->AddStateHandler(
        State::kDestinationBlendAlphaFunctionParamName,
        new BlendFuncStateHandler<AlphaDstFunc>(arg, dirty));
    renderer->AddStateHandler(State::kBlendAlphaEquationParamName,
                              new BlendEqStateHandler<AlphaEq>(arg, dirty));
  }

  // Color Write
  {
    using command_buffer::set_color_write::DitherEnable;
    using command_buffer::set_color_write::AllColorsMask;
    bool *dirty = color_write_helper_.dirty_ptr();
    uint32 *arg = &(color_write_helper_.args()[0].value_uint32);
    renderer->AddStateHandler(State::kDitherEnableParamName,
                              new EnableStateHandler<DitherEnable>(arg, dirty));
    renderer->AddStateHandler(
        State::kColorWriteEnableParamName,
        new BitFieldStateHandler<AllColorsMask>(arg, dirty));
  }
}

void RendererCB::StateManager::ValidateStates(CommandBufferHelper *helper) {
  point_line_helper_.Validate(helper);
  poly_offset_helper_.Validate(helper);
  poly_raster_helper_.Validate(helper);
  alpha_test_helper_.Validate(helper);
  depth_test_helper_.Validate(helper);
  stencil_test_helper_.Validate(helper);
  color_write_helper_.Validate(helper);
  blending_helper_.Validate(helper);
  blending_color_helper_.Validate(helper);
}

}  // namespace o3d
