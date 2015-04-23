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


// This file contains the definition of the Texture2D and TextureCUBE classes.

#include "core/cross/precompile.h"
#include "core/cross/texture.h"
#include "core/cross/bitmap.h"
#include "core/cross/renderer.h"
#include "core/cross/client_info.h"
#include "core/cross/error.h"

namespace o3d {

O3D_DEFN_CLASS(Texture2D, Texture);
O3D_DEFN_CLASS(TextureCUBE, Texture);

const char* Texture2D::kWidthParamName =
    O3D_STRING_CONSTANT("width");
const char* Texture2D::kHeightParamName =
    O3D_STRING_CONSTANT("height");
const char* TextureCUBE::kEdgeLengthParamName =
    O3D_STRING_CONSTANT("edgeLength");

Texture2D::Texture2D(ServiceLocator* service_locator,
                     int width,
                     int height,
                     Format format,
                     int levels,
                     bool alpha_is_one,
                     bool resize_to_pot,
                     bool enable_render_surfaces)
    : Texture(service_locator, format, levels, alpha_is_one,
              resize_to_pot, enable_render_surfaces),
      locked_levels_(0) {
  RegisterReadOnlyParamRef(kWidthParamName, &width_param_);
  RegisterReadOnlyParamRef(kHeightParamName, &height_param_);
  width_param_->set_read_only_value(width);
  height_param_->set_read_only_value(height);

  ClientInfoManager* client_info_manager =
      service_locator->GetService<ClientInfoManager>();
  client_info_manager->AdjustTextureMemoryUsed(
    static_cast<int>(Bitmap::GetMipChainSize(width, height, format, levels)));
}

Texture2D::~Texture2D() {
  if (locked_levels_ != 0) {
    O3D_ERROR(service_locator())
        << "Texture2D \"" << name()
        << "\" was never unlocked before being destroyed.";
  }

  ClientInfoManager* client_info_manager =
      service_locator()->GetService<ClientInfoManager>();
  client_info_manager->AdjustTextureMemoryUsed(
    -static_cast<int>(Bitmap::GetMipChainSize(width(),
                                              height(),
                                              format(),
                                              levels())));
}

ObjectBase::Ref Texture2D::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref();
}

Texture2DLockHelper::Texture2DLockHelper(Texture2D* texture, int level)
    : texture_(texture),
      level_(level),
      data_(NULL),
      locked_(false) {
}

Texture2DLockHelper::~Texture2DLockHelper() {
  if (locked_) {
    texture_->Unlock(level_);
  }
}

void* Texture2DLockHelper::GetData() {
  if (!locked_) {
    locked_ = texture_->Lock(level_, &data_);
    if (!locked_) {
      O3D_ERROR(texture_->service_locator())
          << "Unable to lock buffer '" << texture_->name() << "'";
    }
  }
  return data_;
}

TextureCUBE::TextureCUBE(ServiceLocator* service_locator,
                         int edge_length,
                         Format format,
                         int levels,
                         bool alpha_is_one,
                         bool resize_to_pot,
                         bool enable_render_surfaces)
    : Texture(service_locator, format, levels, alpha_is_one,
              resize_to_pot, enable_render_surfaces) {
  for (unsigned int i = 0; i < 6; ++i) {
    locked_levels_[i] = 0;
  }
  RegisterReadOnlyParamRef(kEdgeLengthParamName, &edge_length_param_);
  edge_length_param_->set_read_only_value(edge_length);

  ClientInfoManager* client_info_manager =
      service_locator->GetService<ClientInfoManager>();
  client_info_manager->AdjustTextureMemoryUsed(
    static_cast<int>(Bitmap::GetMipChainSize(edge_length,
                                             edge_length,
                                             format,
                                             levels)) * 6);
}

TextureCUBE::~TextureCUBE() {
  for (unsigned int i = 0; i < 6; ++i) {
    if (locked_levels_[i] != 0) {
      O3D_ERROR(service_locator())
          << "TextureCUBE \"" << name() << "\" was never unlocked before"
          << "being destroyed.";
      break;  // No need to report it more than once.
    }
  }

  ClientInfoManager* client_info_manager =
      service_locator()->GetService<ClientInfoManager>();
  client_info_manager->AdjustTextureMemoryUsed(
    -static_cast<int>(Bitmap::GetMipChainSize(edge_length(),
                                              edge_length(),
                                              format(),
                                              levels()) * 6));
}

ObjectBase::Ref TextureCUBE::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref();
}

}  // namespace o3d
