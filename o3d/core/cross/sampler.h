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


// This file contains the declaration for the Sampler class.

#ifndef O3D_CORE_CROSS_SAMPLER_H__
#define O3D_CORE_CROSS_SAMPLER_H__

#include "core/cross/param_object.h"
#include "core/cross/param.h"
#include "core/cross/texture.h"

namespace o3d {

// A Sampler is the base class for texture sampler objects.  Texture Samplers
// encapsulate a Texture object reference with states that determine how the
// texture gets used. Samplers keep a reference to the texture object associated
// to them via a param.
class Sampler : public ParamObject {
 public:
  typedef SmartPointer<Sampler> Ref;
  typedef WeakPointer<Sampler> WeakPointerType;

  // Address Modes.
  enum AddressMode {
    WRAP,
    MIRROR,
    CLAMP,
    BORDER
  };

  // Filter types.
  enum FilterType {
    NONE,
    POINT,
    LINEAR,
    ANISOTROPIC
  };

  // Name of Param defining the Texture object used by the Sampler.
  static const char* kTextureParamName;
  // Names of Params that get created for every Sampler.
  static const char* kAddressUModeParamName;
  static const char* kAddressVModeParamName;
  static const char* kAddressWModeParamName;
  static const char* kMagFilterParamName;
  static const char* kMinFilterParamName;
  static const char* kMipFilterParamName;
  static const char* kBorderColorParamName;
  static const char* kMaxAnisotropyParamName;

  explicit Sampler(ServiceLocator* service_locator);

  // Returns the Texture object bound to the Sampler.
  Texture* texture() const {
    return texture_param_ref_->value();
  }

  // Binds a Texture object to the Sampler.
  void set_texture(Texture *texture) {
    texture_param_ref_->set_value(texture);
  }

  AddressMode address_mode_u() const {
    return static_cast<AddressMode>(address_mode_u_param_ref_->value());
  }
  AddressMode address_mode_v() const {
    return static_cast<AddressMode>(address_mode_v_param_ref_->value());
  }
  AddressMode address_mode_w() const {
    return static_cast<AddressMode>(address_mode_w_param_ref_->value());
  }

  FilterType mag_filter() const {
    return static_cast<FilterType>(mag_filter_param_ref_->value());
  }
  FilterType min_filter() const {
    return static_cast<FilterType>(min_filter_param_ref_->value());
  }
  FilterType mip_filter() const {
    return static_cast<FilterType>(mip_filter_param_ref_->value());
  }

  Float4 border_color() const {
    return border_color_param_ref_->value();
  }

  int max_anisotropy() const {
    return max_anisotropy_param_ref_->value();
  }

  void set_address_mode_u(AddressMode mode) {
    address_mode_u_param_ref_->set_value(mode);
  }
  void set_address_mode_v(AddressMode mode) {
    address_mode_v_param_ref_->set_value(mode);
  }
  void set_address_mode_w(AddressMode mode) {
    address_mode_w_param_ref_->set_value(mode);
  }

  void set_mag_filter(FilterType type) {
    mag_filter_param_ref_->set_value(type);
  }
  void set_min_filter(FilterType type) {
    min_filter_param_ref_->set_value(type);
  }
  void set_mip_filter(FilterType type) {
    mip_filter_param_ref_->set_value(type);
  }

  void set_border_color(const Float4& color) {
    border_color_param_ref_->set_value(color);
  }

  void set_max_anisotropy(int max_anisotropy) {
    max_anisotropy_param_ref_->set_value(max_anisotropy);
  }

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

 protected:
  // Creates (if it doesn't already exist) and registers a Param in the
  // Sampler and sets its value to the default_value.
  // Parameters:
  //   param_name: name of the Param to create and register.
  //   default_value: value to set to the Param after it gets created.
  //   typed_param_ref_pointer: pointer to the param reference to be set.
  template<typename T>
  void RegisterAndSetParam(const String& param_name,
                           unsigned int default_value,
                           T* typed_param_ref_pointer);

  // References to all the default Params created for Samplers.
  ParamInteger::Ref address_mode_u_param_ref_;
  ParamInteger::Ref address_mode_v_param_ref_;
  ParamInteger::Ref address_mode_w_param_ref_;
  ParamInteger::Ref mag_filter_param_ref_;
  ParamInteger::Ref min_filter_param_ref_;
  ParamInteger::Ref mip_filter_param_ref_;
  ParamFloat4::Ref border_color_param_ref_;
  ParamInteger::Ref max_anisotropy_param_ref_;

  ParamTexture::Ref texture_param_ref_;

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  O3D_DECL_CLASS(Sampler, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(Sampler);
};

class ParamSampler : public TypedRefParam<Sampler> {
 public:
  typedef SmartPointer<ParamSampler> Ref;

  ParamSampler(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedRefParam<Sampler>(service_locator, dynamic, read_only) {}

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamSampler, RefParamBase);
  DISALLOW_COPY_AND_ASSIGN(ParamSampler);
};


}  // namespace o3d

#endif  // O3D_CORE_CROSS_SAMPLER_H__
