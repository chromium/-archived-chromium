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


// Define the CreateRenderer() factory function for o3d::Renderer that
// creates and returns platform specific implementation for the abstract
// Renderer class.

#include "core/cross/precompile.h"

#include "core/cross/renderer.h"

#include "core/cross/display_window.h"
#include "core/cross/error.h"
#include "core/cross/features.h"
#include "core/cross/param_cache.h"
#include "core/cross/state.h"

namespace o3d {

const InterfaceId Renderer::kInterfaceId =
    InterfaceTraits<Renderer>::kInterfaceId;

namespace {

class ParamErrorSampler : public ParamSampler {
 public:
  explicit ParamErrorSampler(ServiceLocator* service_locator)
      : ParamSampler(service_locator, true, false),
        renderer_(service_locator->GetService<Renderer>()) {
  }
  // This is only called by the renderer and the user can never gain
  // access to a ParamErrorSampler. The only place it is used is it is put in a
  // ParamCache if a matching sampler is not found so that we can easily supply
  // an error texture.
  // If it's called and error_texture on the client is NULL then generate an
  // error.
  virtual void ComputeValue() {
    if (!renderer_->error_texture()) {
      O3D_ERROR(service_locator()) << "Missing ParamSampler";
    }
  }

 private:
  Renderer* renderer_;
};

// Checks if a texture format is supported. If not generates an error.
// Returns true if the texture is supported.
bool IsSupportedTextureFormat(Texture::Format format,
                              Features* features,
                              ServiceLocator* service_locator) {
  if ((format == Texture::R32F ||
       format == Texture::ABGR16F ||
       format == Texture::ABGR32F) &&
      !features->floating_point_textures()) {
    O3D_ERROR(service_locator)
        << "You can not create a floating point texture unless you request "
        << "support for floating point textures when you initialize O3D.";
    return false;
  }
  return true;
}

}  // anonymous namespace

Renderer::Renderer(ServiceLocator* service_locator)
    : service_locator_(service_locator),
      service_(service_locator, this),
      features_(service_locator),
      supports_npot_(false),
      clear_client_(true),
      current_render_surface_(NULL),
      current_depth_surface_(NULL),
      render_frame_count_(0),
      transforms_processed_(0),
      transforms_culled_(0),
      draw_elements_processed_(0),
      draw_elements_culled_(0),
      draw_elements_rendered_(0),
      primitives_rendered_(0),
      width_(0),
      height_(0),
      render_width_(0),
      render_height_(0),
      viewport_(0.0f, 0.0f, 1.0f, 1.0f),
      depth_range_(0.0f, 1.0f),
      dest_x_offset_(0),
      dest_y_offset_(0) {
}

Renderer::~Renderer() {
  // Delete all the state handlers.
  while (!state_handler_map_.empty()) {
    delete state_handler_map_.begin()->second;
    state_handler_map_.erase(state_handler_map_.begin());
  }
}

Renderer::InitStatus Renderer::Init(
    const DisplayWindow& display,
    bool off_screen) {
  if (features_->init_status() != Renderer::SUCCESS) {
    return features_->init_status();
  }

  return InitPlatformSpecific(display, off_screen);
}

void Renderer::InitCommon() {
  AddDefaultStates();
  SetInitialStates();
  error_object_ = ParamObject::Ref(new ParamObject(service_locator_));
  error_sampler_ = CreateSampler();
  Texture2D::Ref texture = CreateTexture2D(8, 8, Texture::XRGB8, 1, false);
  DCHECK(!error_object_.IsNull());
  error_object_->set_name(O3D_STRING_CONSTANT("errorObject"));
  DCHECK(!error_sampler_.IsNull());
  error_sampler_->set_name(O3D_STRING_CONSTANT("errorSampler"));

  // TODO: remove ifdef when textures are implemented on CB
#ifndef RENDERER_CB
  DCHECK(!texture.IsNull());
  texture->set_name(O3D_STRING_CONSTANT("errorTexture"));
  texture->set_alpha_is_one(true);
  void* texture_data;
  bool locked = texture->Lock(0, &texture_data);
  DCHECK(locked);
  static unsigned char error_texture_data[] = {
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00,
    0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00,
    0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00,
    0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00,
    0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
  };
  DCHECK(sizeof error_texture_data ==
         Bitmap::GetBufferSize(texture->width(),
                               texture->height(),
                               texture->format()));
  memcpy(texture_data, error_texture_data, sizeof error_texture_data);
  bool unlocked = texture->Unlock(0);
  DCHECK(unlocked);
#endif

  error_sampler_->set_mag_filter(Sampler::POINT);
  error_sampler_->set_min_filter(Sampler::POINT);
  error_sampler_->set_mip_filter(Sampler::POINT);
  error_param_sampler_ = ParamSampler::Ref(new ParamErrorSampler(
                                               service_locator_));
  DCHECK(!error_param_sampler_.IsNull());
  error_object_->AddParam(O3D_STRING_CONSTANT("errorSampler"),
                          error_param_sampler_);
  error_param_sampler_->set_dynamic_value(error_sampler_);
  SetErrorTexture(texture);
  fallback_error_texture_ = Texture::Ref(texture.Get());
}

void Renderer::UninitCommon() {
  error_param_sampler_.Reset();
  error_sampler_.Reset();
  error_texture_.Reset();
  error_object_.Reset();
  fallback_error_texture_.Reset();
  RemoveDefaultStates();
}

void Renderer::SetLostResourcesCallback(LostResourcesCallback* callback) {
  lost_resources_callback_manager_.Set(callback);
}

void Renderer::ClearLostResourcesCallback() {
  lost_resources_callback_manager_.Clear();
}

void Renderer::SetErrorTexture(Texture* texture) {
  error_texture_ = Texture::Ref(texture);
  error_sampler_->set_texture(
      texture ? texture : fallback_error_texture_.Get());
}

void Renderer::SetClientSize(int width, int height) {
  width_ = width;
  height_ = height;
  render_width_ = width;
  render_height_ = height;
  clear_client_ = true;
}


void Renderer::GetViewport(Float4* viewport, Float2* depth_range) {
  DCHECK(viewport);
  DCHECK(depth_range);
  *viewport = viewport_;
  *depth_range = depth_range_;
}

void Renderer::SetViewport(const Float4& rectangle, const Float2& depth_range) {
  viewport_ = rectangle;
  depth_range_ = depth_range;
  int width = render_width();
  int height = render_height();
  float float_width = static_cast<float>(width);
  float float_height = static_cast<float>(height);


  int viewport_left = static_cast<int>(float_width * rectangle[0] + 0.5f);
  int viewport_top = static_cast<int>(float_height * rectangle[1] + 0.5f);
  int viewport_width = static_cast<int>(float_width * rectangle[2] + 0.5f);
  int viewport_height = static_cast<int>(float_height * rectangle[3] + 0.5f);

  if (viewport_width < 0) {
    O3D_ERROR(service_locator()) << "attempt to set viewport width < 0";
    viewport_width = 0;
  }

  if (viewport_height < 0) {
    O3D_ERROR(service_locator()) << "attempt to set viewport height < 0";
    viewport_height = 0;
  }

  if (viewport_left < 0) {
    O3D_ERROR(service_locator()) << "attempt to set viewport left < 0";
    viewport_left = 0;
  }

  if (viewport_top < 0) {
    O3D_ERROR(service_locator()) << "attempt to set viewport top < 0";
    viewport_top = 0;
  }

  int viewport_right = viewport_left + viewport_width;
  if (viewport_right > width) {
    O3D_ERROR(service_locator()) <<
        "attempt to set viewport left + width to value > 1";
    viewport_width -= viewport_right - width;
    if (viewport_left > width) {
      viewport_left = width;
      viewport_width = 0;
    }
  }

  int viewport_bottom = viewport_top + viewport_height;
  if (viewport_bottom > height) {
    O3D_ERROR(service_locator()) <<
        "attempt to set viewport top + height to value > 1";
    viewport_height -= viewport_bottom - height;
    if (viewport_top > height) {
      viewport_top = height;
      viewport_height = 0;
    }
  }

  SetViewportInPixels(viewport_left,
                      viewport_top,
                      viewport_width,
                      viewport_height,
                      depth_range[0],
                      depth_range[1]);
}

Texture::Ref Renderer::CreateTextureFromBitmap(Bitmap* bitmap) {
  if (!IsSupportedTextureFormat(bitmap->format(),
                                features_.Get(),
                                service_locator())) {
    return Texture::Ref(NULL);
  }
  return CreatePlatformSpecificTextureFromBitmap(bitmap);
}

Texture2D::Ref Renderer::CreateTexture2D(int width,
                                         int height,
                                         Texture::Format format,
                                         int levels,
                                         bool enable_render_surfaces) {
  if (!IsSupportedTextureFormat(format,
                                features_.Get(),
                                service_locator())) {
    return Texture2D::Ref(NULL);
  }
  return CreatePlatformSpecificTexture2D(width,
                                         height,
                                         format,
                                         levels,
                                         enable_render_surfaces);
}

TextureCUBE::Ref Renderer::CreateTextureCUBE(int edge_length,
                                             Texture::Format format,
                                             int levels,
                                             bool enable_render_surfaces) {
  if (!IsSupportedTextureFormat(format,
                                features_.Get(),
                                service_locator())) {
    return TextureCUBE::Ref(NULL);
  }
  return CreatePlatformSpecificTextureCUBE(edge_length,
                                           format,
                                           levels,
                                           enable_render_surfaces);
}

// Creates and returns a ParamCache object
ParamCache* Renderer::CreateParamCache() {
  // TODO: keep a list of free ParamCache objects to avoid allocations.
  return CreatePlatformSpecificParamCache();
}

// Frees a Param Cache.
void Renderer::FreeParamCache(ParamCache* param_cache) {
  // TODO: keep a list of free ParamCache objects to avoid allocations.
  //     Note that in order for that to work there must be a ParamCache::Clear()
  //     function so that an unused ParamCache releases its references to
  //     assets.
  delete param_cache;
}

void Renderer::AddStateHandler(const String& state_name,
                               StateHandler* state_handler) {
  DLOG_ASSERT(state_handler_map_.find(state_name) == state_handler_map_.end())
      << "attempt to add duplicate state handler";
  state_handler->set_index(static_cast<int>(state_handler_map_.size()));
  state_handler_map_.insert(std::make_pair(state_name, state_handler));
  state_param_stacks_.push_back(ParamVector());
}

const ObjectBase::Class* Renderer::GetStateParamType(
    const String& state_name) const {
  StateHandlerMap::const_iterator iter = state_handler_map_.find(state_name);
  return iter != state_handler_map_.end() ? iter->second->GetClass() : NULL;
}

// Set initial render states on device creation/reset.
void Renderer::SetInitialStates() {
  for (StateHandlerMap::iterator it = state_handler_map_.begin();
       it != state_handler_map_.end(); ++it) {
    StateHandler *state_handler = it->second;
    ParamVector& param_stack = state_param_stacks_[state_handler->index()];
    DCHECK_EQ(param_stack.size(), 1);
    state_handler->SetState(this, param_stack[0]);
  }
}

template <class ParamType>
void CreateStateParam(State *state,
                      const String &name,
                      const typename ParamType::DataType &value) {
  ParamType *param = state->GetStateParam<ParamType>(name);
  DCHECK(param);
  param->set_value(value);
}

void Renderer::AddDefaultStates() {
  default_state_ = State::Ref(new State(service_locator_, this));
  default_state_->set_name(O3D_STRING_CONSTANT("defaultState"));

  CreateStateParam<ParamBoolean>(default_state_,
                                 State::kAlphaTestEnableParamName,
                                 false);
  CreateStateParam<ParamFloat>(default_state_,
                               State::kAlphaReferenceParamName,
                               0.f);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kAlphaComparisonFunctionParamName,
                                 State::CMP_ALWAYS);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kCullModeParamName,
                                 State::CULL_CW);
  CreateStateParam<ParamBoolean>(default_state_,
                                 State::kDitherEnableParamName,
                                 false);
  CreateStateParam<ParamBoolean>(default_state_,
                                 State::kLineSmoothEnableParamName,
                                 false);
  CreateStateParam<ParamBoolean>(default_state_,
                                 State::kPointSpriteEnableParamName,
                                 false);
  CreateStateParam<ParamFloat>(default_state_,
                               State::kPointSizeParamName,
                               1.f);
  CreateStateParam<ParamFloat>(default_state_,
                               State::kPolygonOffset1ParamName,
                               0.f);
  CreateStateParam<ParamFloat>(default_state_,
                               State::kPolygonOffset2ParamName,
                               0.f);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kFillModeParamName,
                                 State::SOLID);
  CreateStateParam<ParamBoolean>(default_state_,
                                 State::kZEnableParamName,
                                 true);
  CreateStateParam<ParamBoolean>(default_state_,
                                 State::kZWriteEnableParamName,
                                 true);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kZComparisonFunctionParamName,
                                 State::CMP_LESS);
  CreateStateParam<ParamBoolean>(default_state_,
                                 State::kAlphaBlendEnableParamName,
                                 false);
  CreateStateParam<ParamBoolean>(default_state_,
                                 State::kSeparateAlphaBlendEnableParamName,
                                 false);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kSourceBlendFunctionParamName,
                                 State::BLENDFUNC_ONE);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kDestinationBlendFunctionParamName,
                                 State::BLENDFUNC_ZERO);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kBlendEquationParamName,
                                 State::BLEND_ADD);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kSourceBlendAlphaFunctionParamName,
                                 State::BLENDFUNC_ONE);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kDestinationBlendAlphaFunctionParamName,
                                 State::BLENDFUNC_ZERO);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kBlendAlphaEquationParamName,
                                 State::BLEND_ADD);
  CreateStateParam<ParamBoolean>(default_state_,
                                 State::kStencilEnableParamName,
                                 false);
  CreateStateParam<ParamBoolean>(default_state_,
                                 State::kTwoSidedStencilEnableParamName,
                                 false);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kStencilReferenceParamName,
                                 0);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kStencilMaskParamName,
                                 -1);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kStencilWriteMaskParamName,
                                 -1);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kStencilFailOperationParamName,
                                 State::STENCIL_KEEP);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kStencilZFailOperationParamName,
                                 State::STENCIL_KEEP);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kStencilPassOperationParamName,
                                 State::STENCIL_KEEP);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kStencilComparisonFunctionParamName,
                                 State::CMP_ALWAYS);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kCCWStencilFailOperationParamName,
                                 State::STENCIL_KEEP);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kCCWStencilZFailOperationParamName,
                                 State::STENCIL_KEEP);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kCCWStencilPassOperationParamName,
                                 State::STENCIL_KEEP);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kCCWStencilComparisonFunctionParamName,
                                 State::CMP_ALWAYS);
  CreateStateParam<ParamInteger>(default_state_,
                                 State::kColorWriteEnableParamName,
                                 0xf);
  // Check that we have set every state.
  DCHECK_EQ(default_state_->params().size(), state_param_stacks_.size());

  // Push the default state on the stack, and every handler on their respective
  // handler stack.
  DCHECK(state_stack_.empty());
  state_stack_.push_back(default_state_);
  const NamedParamRefMap& param_map = default_state_->params();
  NamedParamRefMap::const_iterator end(param_map.end());
  for (NamedParamRefMap::const_iterator iter(param_map.begin());
       iter != end;
       ++iter) {
    Param* param = iter->second.Get();
    const StateHandler* state_handler = GetStateHandler(param);
    DCHECK(state_handler);
    ParamVector& param_stack = state_param_stacks_[state_handler->index()];
    DCHECK(param_stack.empty());
    param_stack.push_back(param);
  }
}

void Renderer::RemoveDefaultStates() {
  DCHECK_EQ(state_stack_.size(), 1);
  DCHECK(state_stack_[0] == default_state_);
  state_stack_.clear();
  const NamedParamRefMap& param_map = default_state_->params();
  NamedParamRefMap::const_iterator end(param_map.end());
  for (NamedParamRefMap::const_iterator iter(param_map.begin());
       iter != end;
       ++iter) {
    Param* param = iter->second.Get();
    const StateHandler* state_handler = GetStateHandler(param);
    DCHECK(state_handler);
    ParamVector& param_stack = state_param_stacks_[state_handler->index()];
    DCHECK_EQ(param_stack.size(), 1);
    DCHECK(param_stack[0] == param);
    param_stack.clear();
  }
  default_state_.Reset();
}

const Renderer::StateHandler* Renderer::GetStateHandler(Param* param) const {
  if (param->handle()) {
    return static_cast<const StateHandler *>((param->handle()));
  } else {
    StateHandlerMap::const_iterator state_handler_map_iter =
        state_handler_map_.find(param->name());
    if (state_handler_map_iter != state_handler_map_.end()) {
      StateHandler* state_handler = state_handler_map_iter->second;
      param->set_handle(state_handler);
      return state_handler;
    }
  }
  return NULL;
}

// Pushes rendering states.
void Renderer::PushRenderStates(State *state) {
  DCHECK(!state_stack_.empty());
  if (state && (state_stack_.back() != state)) {
    const NamedParamRefMap& param_map = state->params();
    NamedParamRefMap::const_iterator end(param_map.end());
    for (NamedParamRefMap::const_iterator iter(param_map.begin());
         iter != end;
         ++iter) {
      Param* param = iter->second.Get();
      const StateHandler* state_handler = GetStateHandler(param);
      if (state_handler) {
        state_handler->SetState(this, param);
        state_param_stacks_[state_handler->index()].push_back(param);
      }
    }
  }
  // If the state is null, push top state since that's that state
  // that represents our current situation.
  if (!state) {
    state = state_stack_.back();
  }
  state_stack_.push_back(state);
}

// Pops rendering states to back to their previous settings.
void Renderer::PopRenderStates() {
  DCHECK_GT(state_stack_.size(), 1u);
  if (state_stack_.back() != state_stack_[state_stack_.size() - 2]) {
    State* state = state_stack_.back();
    // restore the states the top state object set.
    const NamedParamRefMap& param_map = state->params();
    NamedParamRefMap::const_iterator end(param_map.end());
    for (NamedParamRefMap::const_iterator iter(param_map.begin());
         iter != end;
         ++iter) {
      Param* param = iter->second.Get();
      const StateHandler* state_handler = GetStateHandler(param);
      if (state_handler) {
        ParamVector& param_stack = state_param_stacks_[state_handler->index()];
        DCHECK(param_stack.back() == param);
        param_stack.pop_back();
        DCHECK(!param_stack.empty());
        state_handler->SetState(this, param_stack.back());
      }
    }
  }
  state_stack_.pop_back();
}

void Renderer::SetRenderSurfaces(RenderSurface* surface,
                                 RenderDepthStencilSurface* depth_surface) {
  if (surface != NULL || depth_surface != NULL) {
    SetRenderSurfacesPlatformSpecific(surface, depth_surface);
    current_render_surface_ = surface;
    current_depth_surface_ = depth_surface;
    if (surface) {
      render_width_ = surface->width();
      render_height_ = surface->height();
    } else {
      render_width_ = depth_surface->width();
      render_height_ = depth_surface->height();
    }
  } else {
    SetBackBufferPlatformSpecific();
    current_render_surface_ = NULL;
    current_depth_surface_ = NULL;
    render_width_ = width();
    render_height_ = height();
  }
  // We must reset the viewport after each change in surfaces.
  SetViewport(viewport_, depth_range_);
}

void Renderer::GetRenderSurfaces(RenderSurface** surface,
                                 RenderDepthStencilSurface** depth_surface) {
  DCHECK(surface);
  DCHECK(depth_surface);
  *surface = current_render_surface_;
  *depth_surface = current_depth_surface_;
}

bool Renderer::SafeToBindTexture(Texture* texture) const {
  if (current_render_surface_ &&
      current_render_surface_->texture() == texture) {
    return false;
  }

  return true;
}

}  // namespace o3d
