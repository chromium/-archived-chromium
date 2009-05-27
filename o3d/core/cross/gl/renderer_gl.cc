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


// This file contains the definition of the RendererGL class that
// implements the abstract Renderer API using OpenGL and the Cg
// Runtime.

#include "core/cross/precompile.h"

#include "core/cross/gl/renderer_gl.h"

#include "core/cross/error.h"
#include "core/cross/gl/buffer_gl.h"
#include "core/cross/gl/draw_element_gl.h"
#include "core/cross/gl/effect_gl.h"
#include "core/cross/gl/param_cache_gl.h"
#include "core/cross/gl/primitive_gl.h"
#include "core/cross/gl/render_surface_gl.h"
#include "core/cross/gl/sampler_gl.h"
#include "core/cross/gl/stream_bank_gl.h"
#include "core/cross/gl/texture_gl.h"
#include "core/cross/gl/utils_gl-inl.h"
#include "core/cross/gl/utils_gl.h"
#include "core/cross/material.h"
#include "core/cross/semantic_manager.h"
#include "core/cross/features.h"
#include "core/cross/shape.h"
#include "core/cross/types.h"

namespace o3d {

namespace {

GLenum ConvertCmpFunc(State::Comparison cmp) {
  switch (cmp) {
    case State::CMP_ALWAYS:
      return GL_ALWAYS;
    case State::CMP_NEVER:
      return GL_NEVER;
    case State::CMP_LESS:
      return GL_LESS;
    case State::CMP_GREATER:
      return GL_GREATER;
    case State::CMP_LEQUAL:
      return GL_LEQUAL;
    case State::CMP_GEQUAL:
      return GL_GEQUAL;
    case State::CMP_EQUAL:
      return GL_EQUAL;
    case State::CMP_NOTEQUAL:
      return GL_NOTEQUAL;
    default:
      break;
  }
  return GL_ALWAYS;
}

GLenum ConvertFillMode(State::Fill mode) {
  switch (mode) {
    case State::POINT:
      return GL_POINT;
    case State::WIREFRAME:
      return GL_LINE;
    case State::SOLID:
      return GL_FILL;
    default:
      break;
  }
  return GL_FILL;
}

GLenum ConvertBlendFunc(State::BlendingFunction blend_func) {
  switch (blend_func) {
    case State::BLENDFUNC_ZERO:
      return GL_ZERO;
    case State::BLENDFUNC_ONE:
      return GL_ONE;
    case State::BLENDFUNC_SOURCE_COLOR:
      return GL_SRC_COLOR;
    case State::BLENDFUNC_INVERSE_SOURCE_COLOR:
      return GL_ONE_MINUS_SRC_COLOR;
    case State::BLENDFUNC_SOURCE_ALPHA:
      return GL_SRC_ALPHA;
    case State::BLENDFUNC_INVERSE_SOURCE_ALPHA:
      return GL_ONE_MINUS_SRC_ALPHA;
    case State::BLENDFUNC_DESTINATION_ALPHA:
      return GL_DST_ALPHA;
    case State::BLENDFUNC_INVERSE_DESTINATION_ALPHA:
      return GL_ONE_MINUS_DST_ALPHA;
    case State::BLENDFUNC_DESTINATION_COLOR:
      return GL_DST_COLOR;
    case State::BLENDFUNC_INVERSE_DESTINATION_COLOR:
      return GL_ONE_MINUS_DST_COLOR;
    case State::BLENDFUNC_SOURCE_ALPHA_SATUTRATE:
      return GL_SRC_ALPHA_SATURATE;
    default:
      break;
  }
  return GL_ONE;
}

GLenum ConvertBlendEquation(State::BlendingEquation blend_equation) {
  switch (blend_equation) {
    case State::BLEND_ADD:
      return GL_FUNC_ADD;
    case State::BLEND_SUBTRACT:
      return GL_FUNC_SUBTRACT;
    case State::BLEND_REVERSE_SUBTRACT:
      return GL_FUNC_REVERSE_SUBTRACT;
    case State::BLEND_MIN:
      return GL_MIN;
    case State::BLEND_MAX:
      return GL_MAX;
    default:
      break;
  }
  return GL_FUNC_ADD;
}

GLenum ConvertStencilOp(State::StencilOperation stencil_func) {
  switch (stencil_func) {
    case State::STENCIL_KEEP:
      return GL_KEEP;
    case State::STENCIL_ZERO:
      return GL_ZERO;
    case State::STENCIL_REPLACE:
      return GL_REPLACE;
    case State::STENCIL_INCREMENT_SATURATE:
      return GL_INCR;
    case State::STENCIL_DECREMENT_SATURATE:
      return GL_DECR;
    case State::STENCIL_INVERT:
      return GL_INVERT;
    case State::STENCIL_INCREMENT:
      return GL_INCR_WRAP;
    case State::STENCIL_DECREMENT:
      return GL_DECR_WRAP;
    default:
      break;
  }
  return GL_KEEP;
}

// Helper routine that will bind the surfaces stored in the RenderSurface and
// RenderDepthStencilSurface arguments to the current OpenGL context.
// Returns true upon success.
// Note:  This routine assumes that a frambuffer object is presently bound
// to the context.
bool InstallFramebufferObjects(RenderSurface* surface,
                               RenderDepthStencilSurface* surface_depth) {
#ifdef _DEBUG
  GLint bound_framebuffer;
  ::glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &bound_framebuffer);
  DCHECK(bound_framebuffer != 0);
#endif

  // Reset the bound attachments to the current framebuffer object.
  ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                 GL_COLOR_ATTACHMENT0_EXT,
                                 GL_RENDERBUFFER_EXT,
                                 0);

  ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                 GL_DEPTH_ATTACHMENT_EXT,
                                 GL_RENDERBUFFER_EXT,
                                 0);

  ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                 GL_STENCIL_ATTACHMENT_EXT,
                                 GL_RENDERBUFFER_EXT,
                                 0);

  if (surface) {
    RenderSurfaceGL *gl_surface = down_cast<RenderSurfaceGL*>(surface);
    Texture *texture = gl_surface->texture();
    if (texture->IsA(Texture2D::GetApparentClass())) {
      ::glFramebufferTexture2DEXT(
          GL_FRAMEBUFFER_EXT,
          GL_COLOR_ATTACHMENT0_EXT,
          GL_TEXTURE_2D,
          reinterpret_cast<GLuint>(texture->GetTextureHandle()),
          gl_surface->mip_level());
    } else if (texture->IsA(TextureCUBE::GetApparentClass())) {
      ::glFramebufferTexture2DEXT(
          GL_FRAMEBUFFER_EXT,
          GL_COLOR_ATTACHMENT0_EXT,
          gl_surface->cube_face(),
          reinterpret_cast<GLuint>(texture->GetTextureHandle()),
          gl_surface->mip_level());
    }
  }

  if (surface_depth) {
    // Bind both the depth and stencil attachments.
    RenderDepthStencilSurfaceGL* gl_surface =
        down_cast<RenderDepthStencilSurfaceGL*>(surface_depth);
    ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                   GL_DEPTH_ATTACHMENT_EXT,
                                   GL_RENDERBUFFER_EXT,
                                   gl_surface->depth_buffer());
    ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                   GL_STENCIL_ATTACHMENT_EXT,
                                   GL_RENDERBUFFER_EXT,
                                   gl_surface->stencil_buffer());
  }
  GLenum framebuffer_status = ::glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if (GL_FRAMEBUFFER_COMPLETE_EXT != framebuffer_status) {
    return false;
  }

  CHECK_GL_ERROR();
  return true;
}

// Helper routine that returns a pointer to the non-NULL entry in the renderer's
// stack of bound surfaces.
const RenderSurfaceBase* GetValidRenderSurface(
    const std::pair<RenderSurface*, RenderDepthStencilSurface*> &stack_entry) {
  if (stack_entry.first) {
    return stack_entry.first;
  } else {
    return stack_entry.second;
  }
}

}  // unnamed namespace

// This class wraps StateHandler to make it typesafe.
template <typename T>
class TypedStateHandler : public RendererGL::StateHandler {
 public:
  // Override this function to set a specific state.
  // Parameters:
  //   renderer: The platform specific renderer.
  //   param: A concrete param with state data.
  virtual void SetStateFromTypedParam(RendererGL* renderer, T* param) const = 0;

  // Gets Class of State's Parameter
  virtual const ObjectBase::Class* GetClass() const {
    return T::GetApparentClass();
  }

 private:
  // Calls SetStateFromTypedParam if the Param type is the correct type.
  // Parameters:
  //   renderer: The platform specific renderer.
  //   param: A param with state data.
  virtual void SetState(Renderer* renderer, Param* param) const {
    RendererGL *renderer_gl = down_cast<RendererGL *>(renderer);
    // This is safe because State guarntees Params match by type.
    DCHECK(param->IsA(T::GetApparentClass()));
    SetStateFromTypedParam(renderer_gl, down_cast<T*>(param));
  }
};

// A template the generates a handler for enable/disable states.
// Parameters:
//   state_constant: GLenum of state we want to enable/disable
template <GLenum state_constant>
class StateEnableHandler : public TypedStateHandler<ParamBoolean> {
 public:
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamBoolean* param) const {
    if (param->value()) {
      ::glEnable(state_constant);
    } else {
      ::glDisable(state_constant);
    }
  }
};

class BoolHandler : public TypedStateHandler<ParamBoolean> {
 public:
  explicit BoolHandler(bool* var, bool* changed_var)
      : var_(*var),
        changed_var_(*changed_var) {
  }
  virtual void SetStateFromTypedParam(RendererGL *renderer,
                                      ParamBoolean *param) const {
    var_ = param->value();
  }
 private:
  bool& var_;
  bool& changed_var_;
};

class ZWriteEnableHandler : public TypedStateHandler<ParamBoolean> {
 public:
  virtual void SetStateFromTypedParam(RendererGL *renderer,
                                      ParamBoolean *param) const {
    ::glDepthMask(param->value());
  }
};

class AlphaReferenceHandler : public TypedStateHandler<ParamFloat> {
 public:
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamFloat* param) const {
    float refFloat = param->value();

    // cap the float to the required range
    if (refFloat < 0.0f) {
      refFloat = 0.0f;
    } else if (refFloat > 1.0f) {
      refFloat = 1.0f;
    }

    renderer->alpha_function_ref_changed_ = true;
    renderer->alpha_ref_ = refFloat;
  }
};

class CullModeHandler : public TypedStateHandler<ParamInteger> {
 public:
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamInteger* param) const {
    State::Cull cull = static_cast<State::Cull>(param->value());
    switch (cull) {
      case State::CULL_CW:
        ::glEnable(GL_CULL_FACE);
        ::glCullFace(GL_BACK);
        break;
      case State::CULL_CCW:
        ::glEnable(GL_CULL_FACE);
        ::glCullFace(GL_FRONT);
        break;
      default:
        ::glDisable(GL_CULL_FACE);
        break;
    }
  }
};

class PolygonOffset1Handler : public TypedStateHandler<ParamFloat> {
 public:
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamFloat* param) const {
    renderer->polygon_offset_factor_ = param->value();
    renderer->polygon_offset_changed_ = true;
  }
};

class PolygonOffset2Handler : public TypedStateHandler<ParamFloat> {
 public:
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamFloat* param) const {
    renderer->polygon_offset_bias_ = param->value();
    renderer->polygon_offset_changed_ = true;
  }
};

class FillModeHandler : public TypedStateHandler<ParamInteger> {
 public:
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamInteger* param) const {
    ::glPolygonMode(GL_FRONT_AND_BACK,
                    ConvertFillMode(static_cast<State::Fill>(param->value())));
  }
};

class ZFunctionHandler : public TypedStateHandler<ParamInteger> {
 public:
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamInteger* param) const {
    ::glDepthFunc(
        ConvertCmpFunc(static_cast<State::Comparison>(param->value())));
  }
};

class BlendEquationHandler : public TypedStateHandler<ParamInteger> {
 public:
  explicit BlendEquationHandler(GLenum* var)
      : var_(*var) {
  }
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamInteger* param) const {
    renderer->alpha_blend_settings_changed_ = true;
    var_ = ConvertBlendEquation(
        static_cast<State::BlendingEquation>(param->value()));
  }
 private:
  GLenum& var_;
};

class BlendFunctionHandler : public TypedStateHandler<ParamInteger> {
 public:
  explicit BlendFunctionHandler(GLenum* var)
      : var_(*var) {
  }
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamInteger* param) const {
    renderer->alpha_blend_settings_changed_ = true;
    var_ = ConvertBlendFunc(
        static_cast<State::BlendingFunction>(param->value()));
  }
 private:
  GLenum& var_;
};


class StencilOperationHandler : public TypedStateHandler<ParamInteger> {
 public:
  StencilOperationHandler(int face, int condition)
      : face_(face) ,
        condition_(condition) {
  }
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamInteger* param) const  {
    renderer->stencil_settings_changed_ = true;
    renderer->stencil_settings_[face_].op_[condition_] = ConvertStencilOp(
        static_cast<State::StencilOperation>(param->value()));
  }
 private:
  int face_;
  int condition_;
};

class ComparisonFunctionHandler : public TypedStateHandler<ParamInteger> {
 public:
  ComparisonFunctionHandler(GLenum* var, bool* changed_var)
      : var_(*var),
        changed_var_(*changed_var) {
  }
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamInteger* param) const {
    changed_var_ = true;
    var_ = ConvertCmpFunc(static_cast<State::Comparison>(param->value()));
  }
 private:
  GLenum& var_;
  bool& changed_var_;
};

class StencilRefHandler : public TypedStateHandler<ParamInteger> {
 public:
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamInteger* param) const {
    renderer->stencil_settings_changed_ = true;
    renderer->stencil_ref_ = param->value();
  }
};

class StencilMaskHandler : public TypedStateHandler<ParamInteger> {
 public:
  explicit StencilMaskHandler(int mask_index)
      : mask_index_(mask_index) {
  }
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamInteger* param) const {
    renderer->stencil_settings_changed_ = true;
    renderer->stencil_mask_[mask_index_] = param->value();
  }
 private:
  int mask_index_;
};

class ColorWriteEnableHandler : public TypedStateHandler<ParamInteger> {
 public:
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamInteger* param) const {
    int mask = param->value();
    ::glColorMask((mask & 0x1) != 0,
                  (mask & 0x2) != 0,
                  (mask & 0x4) != 0,
                  (mask & 0x8) != 0);
  }
};

class PointSpriteEnableHandler : public TypedStateHandler<ParamBoolean> {
 public:
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamBoolean* param) const {
    if (param->value()) {
      ::glEnable(GL_POINT_SPRITE);
      // TODO: It's not clear from D3D docs that point sprites affect
      // TEXCOORD0, but that's my guess. Check that.
      ::glActiveTextureARB(GL_TEXTURE0);
      ::glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
    } else {
      ::glActiveTextureARB(GL_TEXTURE0);
      ::glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_FALSE);
      ::glDisable(GL_POINT_SPRITE);
    }
  }
};

class PointSizeHandler : public TypedStateHandler<ParamFloat> {
 public:
  virtual void SetStateFromTypedParam(RendererGL* renderer,
                                      ParamFloat* param) const {
    ::glPointSize(param->value());
  }
};

RendererGL* RendererGL::CreateDefault(ServiceLocator* service_locator) {
  return new RendererGL(service_locator);
}

RendererGL::RendererGL(ServiceLocator* service_locator)
    : Renderer(service_locator),
      semantic_manager_(service_locator),
#ifdef OS_MACOSX
      mac_agl_context_(0),
      mac_cgl_context_(0),
#endif
#ifdef OS_WIN
      gl_context_(NULL),
#endif
#ifdef OS_LINUX
      display_(NULL),
      window_(0),
      context_(0),
#endif
      fullscreen_(0),
      render_surface_framebuffer_(0),
      cg_context_(NULL),
      alpha_function_ref_changed_(true),
      alpha_function_(GL_ALWAYS),
      alpha_ref_(0.f),
      alpha_blend_settings_changed_(true),
      separate_alpha_blend_enable_(false),
      stencil_settings_changed_(true),
      separate_stencil_settings_enable_(false),
      stencil_ref_(0),
      polygon_offset_changed_(true),
      polygon_offset_factor_(0.f),
      polygon_offset_bias_(0.f) {
  DLOG(INFO) << "RendererGL Construct";

  // Setup default state values.
  for (int ii = 0; ii < 2; ++ii) {
    stencil_settings_[ii].func_ = GL_ALWAYS;
    stencil_settings_[ii].op_[StencilStates::FAIL_OP] = GL_KEEP;
    stencil_settings_[ii].op_[StencilStates::ZFAIL_OP] = GL_KEEP;
    stencil_settings_[ii].op_[StencilStates::PASS_OP] = GL_KEEP;
    stencil_mask_[ii] = -1;
    blend_function_[ii][FRONT] = GL_ONE;
    blend_function_[ii][BACK] = GL_ZERO;
    blend_equation_[ii] = GL_FUNC_ADD;
  }

  // Setup state handlers
  AddStateHandler(State::kAlphaTestEnableParamName,
                  new StateEnableHandler<GL_ALPHA_TEST>);
  AddStateHandler(State::kAlphaReferenceParamName,
                  new AlphaReferenceHandler);
  AddStateHandler(State::kAlphaComparisonFunctionParamName,
                  new ComparisonFunctionHandler(&alpha_function_,
                                                &alpha_function_ref_changed_));
  AddStateHandler(State::kCullModeParamName,
                  new CullModeHandler);
  AddStateHandler(State::kDitherEnableParamName,
                  new StateEnableHandler<GL_DITHER>);
  AddStateHandler(State::kLineSmoothEnableParamName,
                  new StateEnableHandler<GL_LINE_SMOOTH>);
  AddStateHandler(State::kPointSpriteEnableParamName,
                  new PointSpriteEnableHandler);
  AddStateHandler(State::kPointSizeParamName,
                  new PointSizeHandler);
  AddStateHandler(State::kPolygonOffset1ParamName,
                  new PolygonOffset1Handler);
  AddStateHandler(State::kPolygonOffset2ParamName,
                  new PolygonOffset2Handler);
  AddStateHandler(State::kFillModeParamName,
                  new FillModeHandler);
  AddStateHandler(State::kZEnableParamName,
                  new StateEnableHandler<GL_DEPTH_TEST>);
  AddStateHandler(State::kZWriteEnableParamName,
                  new ZWriteEnableHandler);
  AddStateHandler(State::kZComparisonFunctionParamName,
                  new ZFunctionHandler);
  AddStateHandler(State::kAlphaBlendEnableParamName,
                  new StateEnableHandler<GL_BLEND>);
  AddStateHandler(State::kSourceBlendFunctionParamName,
                  new BlendFunctionHandler(&blend_function_[SRC][RGB]));
  AddStateHandler(State::kDestinationBlendFunctionParamName,
                  new BlendFunctionHandler(&blend_function_[DST][RGB]));
  AddStateHandler(State::kStencilEnableParamName,
                  new StateEnableHandler<GL_STENCIL_TEST>);
  AddStateHandler(State::kStencilFailOperationParamName,
                  new StencilOperationHandler(FRONT, StencilStates::FAIL_OP));
  AddStateHandler(State::kStencilZFailOperationParamName,
                  new StencilOperationHandler(FRONT, StencilStates::ZFAIL_OP));
  AddStateHandler(State::kStencilPassOperationParamName,
                  new StencilOperationHandler(FRONT, StencilStates::PASS_OP));
  AddStateHandler(State::kStencilComparisonFunctionParamName,
                  new ComparisonFunctionHandler(
                      &stencil_settings_[FRONT].func_,
                      &stencil_settings_changed_));
  AddStateHandler(State::kStencilReferenceParamName,
                  new StencilRefHandler);
  AddStateHandler(State::kStencilMaskParamName,
                  new StencilMaskHandler(READ_MASK));
  AddStateHandler(State::kStencilWriteMaskParamName,
                  new StencilMaskHandler(WRITE_MASK));
  AddStateHandler(State::kColorWriteEnableParamName,
                  new ColorWriteEnableHandler);
  AddStateHandler(State::kBlendEquationParamName,
                  new BlendEquationHandler(&blend_equation_[RGB]));
  AddStateHandler(State::kTwoSidedStencilEnableParamName,
                  new BoolHandler(&separate_stencil_settings_enable_,
                                  &stencil_settings_changed_));
  AddStateHandler(State::kCCWStencilFailOperationParamName,
                  new StencilOperationHandler(BACK, StencilStates::FAIL_OP));
  AddStateHandler(State::kCCWStencilZFailOperationParamName,
                  new StencilOperationHandler(BACK, StencilStates::ZFAIL_OP));
  AddStateHandler(State::kCCWStencilPassOperationParamName,
                  new StencilOperationHandler(BACK, StencilStates::PASS_OP));
  AddStateHandler(State::kCCWStencilComparisonFunctionParamName,
                  new ComparisonFunctionHandler(
                      &stencil_settings_[BACK].func_,
                      &stencil_settings_changed_));
  AddStateHandler(State::kSeparateAlphaBlendEnableParamName,
                  new BoolHandler(&separate_alpha_blend_enable_,
                                  &alpha_blend_settings_changed_));
  AddStateHandler(State::kSourceBlendAlphaFunctionParamName,
                  new BlendFunctionHandler(&blend_function_[SRC][ALPHA]));
  AddStateHandler(State::kDestinationBlendAlphaFunctionParamName,
                  new BlendFunctionHandler(&blend_function_[DST][ALPHA]));
  AddStateHandler(State::kBlendAlphaEquationParamName,
                  new BlendEquationHandler(&blend_equation_[ALPHA]));
}

RendererGL *RendererGL::current_renderer_ = NULL;

RendererGL::~RendererGL() {
  Destroy();
}

// platform neutral initialization code
//
Renderer::InitStatus RendererGL::InitCommonGL() {
  GLenum glew_error = glewInit();
  if (glew_error != GLEW_OK) {
    DLOG(ERROR) << "Unable to initialise GLEW : "
                << ::glewGetErrorString(glew_error);
    return INITIALIZATION_ERROR;
  }

  // Check to see that we can use the OpenGL vertex attribute APIs
  // TODO:  We should return false if this check fails, but because some
  // Intel hardware does not support OpenGL 2.0, yet does support all of the
  // extensions we require, we only log an error.  A future CL should change
  // this check to ensure that all of the extension strings we require are
  // present.
  if (!GLEW_VERSION_2_0) {
    DLOG(ERROR) << "GL drivers do not have OpenGL 2.0 functionality.";
  }

  if (!GLEW_ARB_vertex_buffer_object) {
    // NOTE: Linux NVidia drivers claim to support OpenGL 2.0 when using
    // indirect rendering (e.g. remote X), but it is actually lying. The
    // ARB_vertex_buffer_object functions silently no-op (!) when using
    // indirect rendering, leading to crashes. Fortunately, in that case, the
    // driver claims to not support ARB_vertex_buffer_object, so fail in that
    // case.
    DLOG(ERROR) << "GL drivers do not support vertex buffer objects.";
    return GPU_NOT_UP_TO_SPEC;
  }

  if (!GLEW_EXT_framebuffer_object) {
    DLOG(ERROR) << "GL drivers do not support framebuffer objects.";
    return GPU_NOT_UP_TO_SPEC;
  }

  supports_npot_ = GLEW_ARB_texture_non_power_of_two;

#ifdef OS_MACOSX
  // The Radeon X1600 says it supports NPOT, but in most situations it doesn't.
  if (supports_npot_ &&
      !strcmp("ATI Radeon X1600 OpenGL Engine",
              reinterpret_cast<const char*>(::glGetString(GL_RENDERER))))
    supports_npot_ = false;
#endif

  // Check for necessary extensions
  if (!GLEW_VERSION_2_0 && !GLEW_EXT_stencil_two_side) {
    DLOG(ERROR) << "Two sided stencil extension missing.";
  }
  if (!GLEW_VERSION_1_4 && !GLEW_EXT_blend_func_separate) {
    DLOG(ERROR) << "Separate blend func extension missing.";
  }
  if (!GLEW_VERSION_2_0 && !GLEW_EXT_blend_equation_separate) {
    DLOG(ERROR) << "Separate blend function extension missing.";
  }
  // create a Cg Runtime.
  cg_context_ = cgCreateContext();
  DLOG_CG_ERROR("Creating Cg context");
  // NOTE: the first CGerror number after the recreation of a
  // CGcontext (the second time through) seems to be trashed. Please
  // ignore any "CG ERROR: Invalid context handle." message on this
  // function - Invalid context handle isn't one of therror states of
  // cgCreateContext().
  DLOG(INFO) << "OpenGL Vendor: " << ::glGetString(GL_VENDOR);
  DLOG(INFO) << "OpenGL Renderer: " << ::glGetString(GL_RENDERER);
  DLOG(INFO) << "OpenGL Version: " << ::glGetString(GL_VERSION);
  DLOG(INFO) << "Cg Version: " << cgGetString(CG_VERSION);
  cg_vertex_profile_ = cgGLGetLatestProfile(CG_GL_VERTEX);
  cgGLSetOptimalOptions(cg_vertex_profile_);
  DLOG(INFO) << "Best Cg vertex profile = "
             << cgGetProfileString(cg_vertex_profile_);
  cg_fragment_profile_ = cgGLGetLatestProfile(CG_GL_FRAGMENT);
  cgGLSetOptimalOptions(cg_fragment_profile_);
  DLOG(INFO) << "Best Cg fragment profile = "
             << cgGetProfileString(cg_fragment_profile_);
  // Set up all Cg State Assignments for OpenGL.
  cgGLRegisterStates(cg_context_);
  DLOG_CG_ERROR("Registering GL StateAssignments");
  cgGLSetDebugMode(CG_FALSE);
  // Enable the profiles we use.
  cgGLEnableProfile(CG_PROFILE_ARBVP1);
  cgGLEnableProfile(CG_PROFILE_ARBFP1);
  // get some limits for this profile.
  GLint max_vertex_attribs = 0;
  ::glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs);
  DLOG(INFO) << "Max Vertex Attribs = " << max_vertex_attribs;
  // Initialize global GL settings.
  // Tell GL that texture buffers can be single-byte aligned.
  ::glPixelStorei(GL_PACK_ALIGNMENT, 1);
  ::glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  ::glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
  ::glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
  CHECK_GL_ERROR();

  GLint viewport[4];
  ::glGetIntegerv(GL_VIEWPORT, &viewport[0]);
  SetClientSize(viewport[2], viewport[3]);
  CHECK_GL_ERROR();

  ::glGenFramebuffersEXT(1, &render_surface_framebuffer_);
  CHECK_GL_ERROR();

  return SUCCESS;
}

// platform neutral destruction code
void RendererGL::DestroyCommonGL() {
  MakeCurrentLazy();
  if (render_surface_framebuffer_) {
    ::glDeleteFramebuffersEXT(1, &render_surface_framebuffer_);
  }

  if (cg_context_) {
    cgDestroyContext(cg_context_);
    cg_context_ = NULL;
  }
}

#ifdef OS_WIN

namespace {

PIXELFORMATDESCRIPTOR kPixelFormatDescriptor = {
  sizeof(kPixelFormatDescriptor),    // Size of structure.
  1,                       // Default version.
  PFD_DRAW_TO_WINDOW |     // Window drawing support.
  PFD_SUPPORT_OPENGL |     // OpenGL support.
  PFD_DOUBLEBUFFER,        // Double buffering support (not stereo).
  PFD_TYPE_RGBA,           // RGBA color mode (not indexed).
  24,                      // 24 bit color mode.
  0, 0, 0, 0, 0, 0,        // Don't set RGB bits & shifts.
  8, 0,                    // 8 bit alpha
  0,                       // No accumulation buffer.
  0, 0, 0, 0,              // Ignore accumulation bits.
  24,                      // 24 bit z-buffer size.
  8,                       // 8-bit stencil buffer.
  0,                       // No aux buffer.
  PFD_MAIN_PLANE,          // Main drawing plane (not overlay).
  0,                       // Reserved.
  0, 0, 0,                 // Layer masks ignored.
};

LRESULT CALLBACK IntermediateWindowProc(HWND window,
                                        UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  return ::DefWindowProc(window, message, w_param, l_param);
}

// Helper routine that returns the highest quality pixel format supported on
// the current platform.  Returns true upon success.
Renderer::InitStatus GetWindowsPixelFormat(HWND window,
                                           Features* features,
                                           int* pixel_format) {
  // We must initialize a GL context before we can determine the multi-sampling
  // supported on the current hardware, so we create an intermediate window
  // and context here.
  HINSTANCE module_handle;
  if (!::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
                           GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           reinterpret_cast<wchar_t*>(IntermediateWindowProc),
                           &module_handle)) {
    return Renderer::INITIALIZATION_ERROR;
  }

  WNDCLASS intermediate_class;
  intermediate_class.style = CS_HREDRAW | CS_VREDRAW;
  intermediate_class.lpfnWndProc = IntermediateWindowProc;
  intermediate_class.cbClsExtra = 0;
  intermediate_class.cbWndExtra = 0;
  intermediate_class.hInstance = module_handle;
  intermediate_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  intermediate_class.hCursor = LoadCursor(NULL, IDC_ARROW);
  intermediate_class.hbrBackground = NULL;
  intermediate_class.lpszMenuName = NULL;
  intermediate_class.lpszClassName = L"Intermediate GL Window";

  ATOM class_registration = ::RegisterClass(&intermediate_class);
  if (!class_registration) {
    return Renderer::INITIALIZATION_ERROR;
  }

  HWND intermediate_window = ::CreateWindow(
      reinterpret_cast<wchar_t*>(class_registration),
      L"",
      WS_OVERLAPPEDWINDOW,
      0, 0,
      CW_USEDEFAULT, CW_USEDEFAULT,
      NULL,
      NULL,
      NULL,
      NULL);

  if (!intermediate_window) {
    ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                      module_handle);
    return Renderer::INITIALIZATION_ERROR;
  }

  HDC intermediate_dc = ::GetDC(intermediate_window);
  int format_index = ::ChoosePixelFormat(intermediate_dc,
                                         &kPixelFormatDescriptor);
  if (format_index == 0) {
    DLOG(ERROR) << "Unable to get the pixel format for GL context.";
    ::ReleaseDC(intermediate_window, intermediate_dc);
    ::DestroyWindow(intermediate_window);
    ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                      module_handle);
    return Renderer::INITIALIZATION_ERROR;
  }
  if (!::SetPixelFormat(intermediate_dc, format_index,
                        &kPixelFormatDescriptor)) {
    DLOG(ERROR) << "Unable to set the pixel format for GL context.";
    ::ReleaseDC(intermediate_window, intermediate_dc);
    ::DestroyWindow(intermediate_window);
    ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                      module_handle);
    return Renderer::INITIALIZATION_ERROR;
  }

  // Store the pixel format without multisampling.
  *pixel_format = format_index;
  HGLRC gl_context = ::wglCreateContext(intermediate_dc);
  if (::wglMakeCurrent(intermediate_dc, gl_context)) {
    // GL context was successfully created and applied to the window's DC.
    // Startup GLEW, the GL extensions wrangler.
    GLenum glew_error = ::glewInit();
    if (glew_error == GLEW_OK) {
      DLOG(INFO) << "Initialized GLEW " << ::glewGetString(GLEW_VERSION);
    } else {
      DLOG(ERROR) << "Unable to initialise GLEW : "
                  << ::glewGetErrorString(glew_error);
      ::wglMakeCurrent(intermediate_dc, NULL);
      ::wglDeleteContext(gl_context);
      ::ReleaseDC(intermediate_window, intermediate_dc);
      ::DestroyWindow(intermediate_window);
      ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                        module_handle);
      return Renderer::INITIALIZATION_ERROR;
    }

    // If the multi-sample extensions are present, query the api to determine
    // the pixel format.
    if (!features->not_anti_aliased() &&
        WGLEW_ARB_pixel_format && WGLEW_ARB_multisample) {
      int pixel_attributes[] = {
        WGL_SAMPLES_ARB, 4,
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_COLOR_BITS_ARB, 24,
        WGL_ALPHA_BITS_ARB, 8,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
        0, 0};

      float pixel_attributes_f[] = {0, 0};
      int msaa_pixel_format;
      unsigned int num_formats;

      // Query for the highest sampling rate supported, starting at 4x.
      static const int kSampleCount[] = {4, 2};
      static const int kNumSamples = 2;
      for (int sample = 0; sample < kNumSamples; ++sample) {
        pixel_attributes[1] = kSampleCount[sample];
        if (GL_TRUE == ::wglChoosePixelFormatARB(intermediate_dc,
                                                 pixel_attributes,
                                                 pixel_attributes_f,
                                                 1,
                                                 &msaa_pixel_format,
                                                 &num_formats)) {
          *pixel_format = msaa_pixel_format;
          break;
        }
      }
    }
  }

  ::wglMakeCurrent(intermediate_dc, NULL);
  ::wglDeleteContext(gl_context);
  ::ReleaseDC(intermediate_window, intermediate_dc);
  ::DestroyWindow(intermediate_window);
  ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                    module_handle);
  return Renderer::SUCCESS;
}

}  // unnamed namespace

Renderer::InitStatus RendererGL::InitPlatformSpecific(
    const DisplayWindow& display,
    bool off_screen) {
  const DisplayWindowWindows &display_platform =
      static_cast<const DisplayWindowWindows&>(display);

  DLOG(INFO) << "RendererGL Init";

  // TODO: Add support for off-screen rendering using OpenGL.
  if (off_screen) {
    return INITIALIZATION_ERROR;
  }

  int pixel_format;
  InitStatus init_status;

  init_status = GetWindowsPixelFormat(display_platform.hwnd(),
                                      features(),
                                      &pixel_format);
  if (init_status != SUCCESS) {
      return init_status;
  }

  window_ = display_platform.hwnd();
  device_context_ = ::GetDC(window_);
  if (!::SetPixelFormat(device_context_, pixel_format,
                        &kPixelFormatDescriptor)) {
    DLOG(ERROR) << "Unable to set the pixel format for GL context.";
    return INITIALIZATION_ERROR;
  }

  gl_context_ = ::wglCreateContext(device_context_);
  if (MakeCurrent()) {
    // Ensure that glew has been initialized for the created rendering context.
    init_status = InitCommonGL();
    if (init_status != SUCCESS) {
      DLOG(ERROR) << "Failed to initialize GL rendering context.";
      return init_status;
    }
    if (WGLEW_ARB_multisample) {
      ::glEnable(GL_MULTISAMPLE_ARB);
    }
  } else {
    DLOG(ERROR) << "Failed to create the GL Context.";
    return INITIALIZATION_ERROR;
  }
  CHECK_GL_ERROR();
  return SUCCESS;
}

// Releases the Cg Context and deletes the GL device.
void RendererGL::Destroy() {
  DLOG(INFO) << "Destroy RendererGL";
  DestroyCommonGL();
  if (device_context_) {
    CHECK_GL_ERROR();
    // Release the OpenGL rendering context.
    ::wglMakeCurrent(device_context_, NULL);
    if (gl_context_) {
      ::wglDeleteContext(gl_context_);
      gl_context_ = NULL;
    }
    // release the hDC obtained through GetDC().
    ::ReleaseDC(window_, device_context_);
    device_context_ = NULL;
    window_ = NULL;
  }
  DLOG(INFO) << "Renderer destroyed.";
}

#endif  // OS_WIN

#ifdef OS_MACOSX

Renderer::InitStatus  RendererGL::InitPlatformSpecific(
    const DisplayWindow& display,
    bool /*off_screen*/) {
  const DisplayWindowMac &display_platform =
      static_cast<const DisplayWindowMac&>(display);
  // TODO: Add support for off screen rendering on the Mac.
  mac_agl_context_ = display_platform.agl_context();
  mac_cgl_context_ = display_platform.cgl_context();

  return InitCommonGL();
}

void RendererGL::Destroy() {
  DestroyCommonGL();
  // We only have to destroy agl contexts,
  // cgl contexts are not owned by us.
  if (mac_agl_context_) {
    ::aglDestroyContext(mac_agl_context_);
    mac_agl_context_ = NULL;
  }
}

#endif  // OS_MACOSX

#ifdef OS_LINUX
Renderer::InitStatus  RendererGL::InitPlatformSpecific(
    const DisplayWindow& display_window,
    bool off_screen) {
  const DisplayWindowLinux &display_platform =
      static_cast<const DisplayWindowLinux&>(display_window);
  Display *display = display_platform.display();
  Window window = display_platform.window();
  XWindowAttributes attributes;
  ::XGetWindowAttributes(display, window, &attributes);
  XVisualInfo visual_info_template;
  visual_info_template.visualid = ::XVisualIDFromVisual(attributes.visual);
  int visual_info_count = 0;
  XVisualInfo *visual_info_list = ::XGetVisualInfo(display, VisualIDMask,
                                                   &visual_info_template,
                                                   &visual_info_count);
  DCHECK(visual_info_list);
  DCHECK_GT(visual_info_count, 0);
  context_ = 0;
  for (int i = 0; i < visual_info_count; ++i) {
    context_ = ::glXCreateContext(display, visual_info_list + i, 0,
                                  True);
    if (context_) break;
  }
  ::XFree(visual_info_list);
  if (!context_) {
    DLOG(ERROR) << "Couldn't create GL context.";
    return INITIALIZATION_ERROR;
  }
  display_ = display;
  window_ = window;
  if (!MakeCurrent()) {
    ::glXDestroyContext(display, context_);
    context_ = 0;
    display_ = NULL;
    window_ = 0;
    DLOG(ERROR) << "Couldn't create GL context.";
    return INITIALIZATION_ERROR;
  }

  InitStatus init_status = InitCommonGL();
  if (init_status != SUCCESS) {
    ::glXDestroyContext(display, context_);
    context_ = 0;
    display_ = NULL;
    window_ = 0;
  }
  return init_status;
}

void RendererGL::Destroy() {
  DestroyCommonGL();
  if (display_) {
    ::glXMakeCurrent(display_, 0, 0);
    current_renderer_ = NULL;
    if (context_) {
      ::glXDestroyContext(display_, context_);
      context_ = 0;
    }
    display_ = NULL;
    window_ = 0;
  }
}

#endif

bool RendererGL::MakeCurrent() {
#ifdef OS_WIN
  if (!device_context_ || !gl_context_) return false;
  bool result = ::wglMakeCurrent(device_context_, gl_context_);
  if (result) current_renderer_ = this;
  return result;
#endif
#ifdef OS_MACOSX
  if (mac_cgl_context_ != NULL) {
    ::CGLSetCurrentContext(mac_cgl_context_);
    current_renderer_ = this;
    return true;
  } else if (mac_agl_context_ != NULL) {
    ::aglSetCurrentContext(mac_agl_context_);
    current_renderer_ = this;
    return true;
  } else {
    return false;
  }
#endif
#ifdef OS_LINUX
  if (context_ != NULL) {
    bool result = ::glXMakeCurrent(display_, window_, context_) == True;
    if (result) current_renderer_ = this;
    return result;
  } else {
    return false;
  }
#endif
}

void RendererGL::Clear(const Float4 &color,
                       bool color_flag,
                       float depth,
                       bool depth_flag,
                       int stencil,
                       bool stencil_flag) {
  MakeCurrentLazy();
  SetChangedStates();
  ::glClearColor(color[0], color[1], color[2], color[3]);
  ::glClearDepth(depth);
  ::glClearStencil(stencil);

  ::glClear((color_flag   ? GL_COLOR_BUFFER_BIT   : 0) |
            (depth_flag   ? GL_DEPTH_BUFFER_BIT   : 0) |
            (stencil_flag ? GL_STENCIL_BUFFER_BIT : 0));
  CHECK_GL_ERROR();
}

// Updates the helper constant used for the D3D -> GL remapping.
// See effect_gl.cc for details.
void RendererGL::UpdateHelperConstant(float width, float height) {
  MakeCurrentLazy();
  // If render-targets are active, pass -1 to invert the Y axis.  OpenGL uses
  // a different viewport orientation than DX.  Without the inversion, the
  // output of render-target rendering will be upside down.
  if (RenderSurfaceActive()) {
    ::glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB,
                                 0,
                                 1.0f / width,
                                 -1.0f / height,
                                 2.0f,
                                 -1.0f);
  } else {
    // Only apply the origin offset when rendering to the client area.
    ::glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB,
                                 0,
                                 (1.0f + (2.0f * -dest_x_offset())) / width,
                                 (-1.0f + (2.0f * dest_y_offset())) / height,
                                 2.0f,
                                 1.0f);
  }
  CHECK_GL_ERROR();
}

void RendererGL::SetViewportInPixels(int left,
                                     int top,
                                     int width,
                                     int height,
                                     float min_z,
                                     float max_z) {
  MakeCurrentLazy();
  int vieport_top =
      RenderSurfaceActive() ? top : render_height() - top - height;
  ::glViewport(left, vieport_top, width, height);
  UpdateHelperConstant(width, height);

  // If it's the full client area turn off scissor test for speed.
  if (left == 0 &&
      top == 0 &&
      width == render_width() &&
      height == render_height()) {
    ::glDisable(GL_SCISSOR_TEST);
  } else {
    ::glScissor(left, vieport_top, width, height);
    ::glEnable(GL_SCISSOR_TEST);
  }
  ::glDepthRange(min_z, max_z);
}

// Resizes the viewport.
void RendererGL::Resize(int width, int height) {
  MakeCurrentLazy();
  SetClientSize(width, height);
  CHECK_GL_ERROR();
}

bool RendererGL::SetFullscreen(bool fullscreen,
                               const DisplayWindow& display,
                               int mode_id) {
  if (fullscreen != fullscreen_) {
    fullscreen_ = fullscreen;
  }
  return true;
}

bool RendererGL::StartRendering() {
  DLOG_FIRST_N(INFO, 10) << "RendererGL StartRendering";
  MakeCurrentLazy();
  ++render_frame_count_;
  transforms_culled_ = 0;
  transforms_processed_ = 0;
  draw_elements_culled_ = 0;
  draw_elements_processed_ = 0;
  draw_elements_rendered_ = 0;
  primitives_rendered_ = 0;

  // Clear the client if we need to.
  if (clear_client_) {
    clear_client_ = false;
    Clear(Float4(0.5f, 0.5f, 0.5f, 1.0f), true, 1.0f, true, 0, true);
  }

  // Currently always returns true.
  // Should be modified if current behavior changes.
  CHECK_GL_ERROR();
  return true;
}

// Clears the color, depth and stncil buffers and prepares GL for rendering
// the frame.
// Returns true on success.
bool RendererGL::BeginDraw() {
  DLOG_FIRST_N(INFO, 10) << "RendererGL BeginDraw";
  MakeCurrentLazy();

  // Reset the viewport.
  SetViewport(Float4(0.0f, 0.0f, 1.0f, 1.0f), Float2(0.0f, 1.0f));

  // Currently always returns true.
  // Should be modified if current behavior changes.
  CHECK_GL_ERROR();
  return true;
}

// Asks the primitive to draw itself.
void RendererGL::RenderElement(Element* element,
                               DrawElement* draw_element,
                               Material* material,
                               ParamObject* override,
                               ParamCache* param_cache) {
  DCHECK(IsCurrent());
  DLOG_FIRST_N(INFO, 10) << "RendererGL RenderElement";
  ++draw_elements_rendered_;
  State *current_state = material ? material->state() : NULL;
  PushRenderStates(current_state);
  SetChangedStates();
  element->Render(this, draw_element, material, override, param_cache);
  PopRenderStates();
  CHECK_GL_ERROR();
}

// Assign the surface arguments to the renderer, and update the stack
// of pushed surfaces.
void RendererGL::SetRenderSurfacesPlatformSpecific(
    RenderSurface* surface,
    RenderDepthStencilSurface* surface_depth) {
  // TODO:  This routine re-uses a single global framebuffer object for
  // all RenderSurface rendering.  Because of the validation checks performed
  // at attachment-change time, it may be more performant to create a pool
  // of framebuffer objects with different attachment characterists and
  // switch between them here.
  MakeCurrentLazy();
  ::glBindFramebufferEXT(GL_FRAMEBUFFER, render_surface_framebuffer_);
  if (!InstallFramebufferObjects(surface, surface_depth)) {
    O3D_ERROR(service_locator())
        << "Failed to bind OpenGL render target objects:"
        << surface->name() <<", "<< surface_depth->name();
  }
  // RenderSurface rendering is performed with an inverted Y, so the front
  // face winding must be changed to clock-wise.  See comments for
  // UpdateHelperConstant.
  glFrontFace(GL_CW);
}

void RendererGL::SetBackBufferPlatformSpecific() {
  MakeCurrentLazy();
  // Bind the default context, and restore the default front-face winding.
  ::glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  glFrontFace(GL_CCW);
}

// Executes a post rendering step
void RendererGL::EndDraw() {
  DLOG_FIRST_N(INFO, 10) << "RendererGL EndDraw";
  DCHECK(IsCurrent());
  SetChangedStates();
}

// Swaps the buffers.
void RendererGL::FinishRendering() {
  DLOG_FIRST_N(INFO, 10) << "RendererGL Present";
  DCHECK(IsCurrent());
  SetChangedStates();
  ::glFlush();
  CHECK_GL_ERROR();
#ifdef OS_WIN
  ::SwapBuffers(device_context_);
#endif
#ifdef OS_LINUX
  ::glXSwapBuffers(display_, window_);
#endif
}

StreamBank::Ref RendererGL::CreateStreamBank() {
  return StreamBank::Ref(new StreamBankGL(service_locator()));
}

Primitive::Ref RendererGL::CreatePrimitive() {
  return Primitive::Ref(new PrimitiveGL(service_locator()));
}

DrawElement::Ref RendererGL::CreateDrawElement() {
  return DrawElement::Ref(new DrawElementGL(service_locator()));
}

void RendererGL::SetStencilStates(GLenum face,
                                  const StencilStates& stencil_state) {
  DCHECK(IsCurrent());
  if (face == GL_FRONT_AND_BACK) {
    ::glStencilFunc(stencil_state.func_,
                    stencil_ref_,
                    stencil_mask_[READ_MASK]);
    ::glStencilOp(stencil_state.op_[StencilStates::FAIL_OP],
                  stencil_state.op_[StencilStates::ZFAIL_OP],
                  stencil_state.op_[StencilStates::PASS_OP]);
    ::glStencilMask(stencil_mask_[WRITE_MASK]);
  } else if (GLEW_VERSION_2_0) {
    ::glStencilFuncSeparate(face,
                            stencil_state.func_,
                            stencil_ref_,
                            stencil_mask_[READ_MASK]);
    ::glStencilOpSeparate(face,
                          stencil_state.op_[StencilStates::FAIL_OP],
                          stencil_state.op_[StencilStates::ZFAIL_OP],
                          stencil_state.op_[StencilStates::PASS_OP]);
    ::glStencilMaskSeparate(face,
                            stencil_mask_[WRITE_MASK]);
  } else if (GLEW_EXT_stencil_two_side) {
    ::glEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);
    ::glActiveStencilFaceEXT(face);
    ::glStencilFunc(stencil_state.func_,
                    stencil_ref_,
                    stencil_mask_[READ_MASK]);
    ::glStencilOp(stencil_state.op_[StencilStates::FAIL_OP],
                  stencil_state.op_[StencilStates::ZFAIL_OP],
                  stencil_state.op_[StencilStates::PASS_OP]);
    ::glStencilMask(stencil_mask_[WRITE_MASK]);
    ::glDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
  }
  CHECK_GL_ERROR();
}

void RendererGL::SetChangedStates() {
  DCHECK(IsCurrent());
  // Set blend settings.
  if (alpha_blend_settings_changed_) {
    if (separate_alpha_blend_enable_) {
      if (GLEW_VERSION_1_4) {
        ::glBlendFuncSeparate(blend_function_[SRC][RGB],
                              blend_function_[DST][RGB],
                              blend_function_[SRC][ALPHA],
                              blend_function_[DST][ALPHA]);
      } else if (GLEW_EXT_blend_func_separate) {
        ::glBlendFuncSeparateEXT(blend_function_[SRC][RGB],
                                 blend_function_[DST][RGB],
                                 blend_function_[SRC][ALPHA],
                                 blend_function_[DST][ALPHA]);
      }
      if (GLEW_VERSION_2_0) {
        ::glBlendEquationSeparate(blend_equation_[RGB],
                                  blend_equation_[ALPHA]);
      } else if (GLEW_EXT_blend_equation_separate) {
        ::glBlendEquationSeparateEXT(blend_equation_[RGB],
                                     blend_equation_[ALPHA]);
      }
    } else {
      ::glBlendFunc(blend_function_[SRC][RGB],
                    blend_function_[DST][RGB]);
      if (::glBlendEquation != NULL)
        ::glBlendEquation(blend_equation_[RGB]);
    }
    alpha_blend_settings_changed_ = false;
  }

  // Set alpha settings.
  if (alpha_function_ref_changed_) {
    ::glAlphaFunc(alpha_function_, alpha_ref_);
    alpha_function_ref_changed_ = false;
  }

  // Set stencil settings.
  if (stencil_settings_changed_) {
    if (separate_stencil_settings_enable_) {
      SetStencilStates(GL_FRONT, stencil_settings_[FRONT]);
      SetStencilStates(GL_BACK, stencil_settings_[BACK]);
    } else {
      SetStencilStates(GL_FRONT_AND_BACK, stencil_settings_[FRONT]);
    }
    stencil_settings_changed_ = false;
  }

  if (polygon_offset_changed_) {
    bool enable = (polygon_offset_factor_ != 0.f) ||
                  (polygon_offset_bias_ != 0.f);
    if (enable) {
      ::glEnable(GL_POLYGON_OFFSET_POINT);
      ::glEnable(GL_POLYGON_OFFSET_LINE);
      ::glEnable(GL_POLYGON_OFFSET_FILL);
      ::glPolygonOffset(polygon_offset_factor_, polygon_offset_bias_);
    } else {
      ::glDisable(GL_POLYGON_OFFSET_POINT);
      ::glDisable(GL_POLYGON_OFFSET_LINE);
      ::glDisable(GL_POLYGON_OFFSET_FILL);
    }
    polygon_offset_changed_ = false;
  }
  CHECK_GL_ERROR();
}

VertexBuffer::Ref RendererGL::CreateVertexBuffer() {
  DLOG(INFO) << "RendererGL CreateVertexBuffer";
  MakeCurrentLazy();
  return VertexBuffer::Ref(new VertexBufferGL(service_locator()));
}

IndexBuffer::Ref RendererGL::CreateIndexBuffer() {
  DLOG(INFO) << "RendererGL CreateIndexBuffer";
  MakeCurrentLazy();
  return IndexBuffer::Ref(new IndexBufferGL(service_locator()));
}

Effect::Ref RendererGL::CreateEffect() {
  DLOG(INFO) << "RendererGL CreateEffect";
  MakeCurrentLazy();
  return Effect::Ref(new EffectGL(service_locator(), cg_context_));
}

Sampler::Ref RendererGL::CreateSampler() {
  return Sampler::Ref(new SamplerGL(service_locator()));
}

ParamCache* RendererGL::CreatePlatformSpecificParamCache() {
  return new ParamCacheGL(semantic_manager_.Get(), this);
}


// Attempts to create a Texture with the given bitmap, automatically
// determining whether the to create a 2D texture, cube texture, etc. If
// creation fails the method returns NULL.
// Parameters:
//  bitmap: The bitmap specifying the dimensions, format and content of the
//          new texture. The created texture takes ownership of the bitmap
//          data.
// Returns:
//  A ref-counted pointer to the texture or NULL if it did not load.
Texture::Ref RendererGL::CreatePlatformSpecificTextureFromBitmap(
    Bitmap* bitmap) {
  if (bitmap->is_cubemap()) {
    return Texture::Ref(TextureCUBEGL::Create(service_locator(),
                                              bitmap,
                                              false));
  } else {
    return Texture::Ref(Texture2DGL::Create(service_locator(),
                                            bitmap,
                                            false));
  }
}

Texture2D::Ref RendererGL::CreatePlatformSpecificTexture2D(
    int width,
    int height,
    Texture::Format format,
    int levels,
    bool enable_render_surfaces) {
  DLOG(INFO) << "RendererGL CreateTexture2D";
  MakeCurrentLazy();
  Bitmap bitmap;
  bitmap.set_format(format);
  bitmap.set_width(width);
  bitmap.set_height(height);
  bitmap.set_num_mipmaps(levels);
  return Texture2D::Ref(Texture2DGL::Create(service_locator(),
                                            &bitmap,
                                            enable_render_surfaces));
}

TextureCUBE::Ref RendererGL::CreatePlatformSpecificTextureCUBE(
    int edge_length,
    Texture::Format format,
    int levels,
    bool enable_render_surfaces) {
  DLOG(INFO) << "RendererGL CreateTextureCUBE";
  MakeCurrentLazy();
  Bitmap bitmap;
  bitmap.set_format(format);
  bitmap.set_width(edge_length);
  bitmap.set_height(edge_length);
  bitmap.set_num_mipmaps(levels);
  bitmap.set_is_cubemap(true);
  return TextureCUBE::Ref(TextureCUBEGL::Create(service_locator(),
                                                &bitmap,
                                                enable_render_surfaces));
}

RenderDepthStencilSurface::Ref RendererGL::CreateDepthStencilSurface(
    int width,
    int height) {
  return RenderDepthStencilSurface::Ref(
      new RenderDepthStencilSurfaceGL(service_locator(),
                                      width,
                                      height));
}

// Saves a png screenshot 'file_name.png'.
// Returns true on success and false on failure.
bool RendererGL::SaveScreen(const String& file_name) {
#ifdef TESTING
  MakeCurrentLazy();
  Bitmap bitmap;
  bitmap.Allocate(Texture::ARGB8, width(), height(), 1, false);
  ::glReadPixels(0, 0, width(), height(), GL_BGRA, GL_UNSIGNED_BYTE,
                 bitmap.image_data());
  bool result = bitmap.SaveToPNGFile((file_name + ".png").c_str());
  if (!result) {
    O3D_ERROR(service_locator())
        << "Failed to save screen into " << file_name;
  }
  return result;
#else
  // Not a test build, always return false.
  return false;
#endif
}

const int* RendererGL::GetRGBAUByteNSwizzleTable() {
  static int swizzle_table[] = { 0, 1, 2, 3, };
  return swizzle_table;
}

// This is a factory function for creating Renderer objects.  Since
// we're implementing GL, we only ever return a GL renderer.
Renderer* Renderer::CreateDefaultRenderer(ServiceLocator* service_locator) {
  return RendererGL::CreateDefault(service_locator);
}

}  // namespace o3d
