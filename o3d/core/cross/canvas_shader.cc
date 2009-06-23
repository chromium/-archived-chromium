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


// This file contains the implementation for the CanvasShader class.

#include "core/cross/precompile.h"
#include "core/cross/canvas_shader.h"
#include "base/scoped_ptr.h"
#include "core/cross/canvas_utils.h"
#include "core/cross/error.h"

#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

namespace o3d {

O3D_DEFN_CLASS(CanvasShader, ParamObject);
O3D_DEFN_CLASS(CanvasLinearGradient, CanvasShader);

static SkShader::TileMode ToSKTileMode(CanvasShader::TileMode mode) {
  switch (mode) {
    case CanvasShader::CLAMP:
      return SkShader::kClamp_TileMode;
    case CanvasShader::MIRROR:
      return SkShader::kMirror_TileMode;
    case CanvasShader::REPEAT:
      return SkShader::kRepeat_TileMode;
    default:
      return SkShader::kRepeat_TileMode;
  }
}

CanvasShader::CanvasShader(ServiceLocator* service_locator)
    : ParamObject(service_locator),
      native_shader_(NULL) {
}

CanvasShader::~CanvasShader() {
  if (native_shader_)
    native_shader_->unref();
}

SkShader* CanvasShader::GetNativeShader() {
  if (needs_update_) {
    needs_update_ = false;
    if (native_shader_)
      native_shader_->unref();
    native_shader_ = MakeNativeShader();
  }
  return native_shader_;
}

CanvasLinearGradient::CanvasLinearGradient(ServiceLocator* service_locator)
    : CanvasShader(service_locator),
      start_point_(Float2(0, 0)),
      end_point_(Float2(0, 0)),
      tile_mode_(REPEAT) {
  colors_.push_back(Float4(0, 0, 0, 1));
  colors_.push_back(Float4(0, 0, 0, 1));
  positions_.clear();
}

CanvasLinearGradient::~CanvasLinearGradient() {
}

SkShader* CanvasLinearGradient::MakeNativeShader() {
  SkPoint start_to_end[2];
  start_to_end[0].set(SkFloatToScalar(start_point_.getX()),
                      SkFloatToScalar(start_point_.getY()));
  start_to_end[1].set(SkFloatToScalar(end_point_.getX()),
                      SkFloatToScalar(end_point_.getY()));

  if (colors_.size() < 2) {
    O3D_ERROR(service_locator()) << "Must provide at least two colors for"
                                 << " CanvasLinearGradient!";
    return NULL;
  }
  scoped_ptr<SkColor> colors(new SkColor[colors_.size()]);
  for (std::vector<Float4>::size_type ii = 0; ii < colors_.size(); ii++) {
    (colors.get())[ii] = Float4ToSkColor(colors_[ii]);
  }
  scoped_ptr<SkScalar> positions;
  if (positions_.size()) {
    if (positions_.size() != colors_.size()) {
      O3D_ERROR(service_locator()) << "The number of positions must match "
                                   << "the number of colors for"
                                   << " CanvasLinearGradient.";
      return NULL;
    }
    positions.reset(new SkScalar[positions_.size()]);
    for (std::vector<float>::size_type ii = 0; ii < positions_.size(); ii++) {
      (positions.get())[ii] = SkFloatToScalar(positions_[ii]);
    }
  } else {
    positions.reset(NULL);
  }

  SkShader* shader = SkGradientShader::CreateLinear(
      start_to_end,
      colors.get(),
      positions.get(),
      colors_.size(),
      ToSKTileMode(tile_mode_));

  if (shader == NULL) {
    DLOG(ERROR) << "Failed to create SkGradientShader (linear)";
  }

  return shader;
}


ObjectBase::Ref CanvasLinearGradient::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new CanvasLinearGradient(service_locator));
}

}  // namespace o3d
