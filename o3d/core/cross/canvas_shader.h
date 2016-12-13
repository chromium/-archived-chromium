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


// This file contains the declaration for the CanvasShader class.

#ifndef O3D_CORE_CROSS_CANVAS_SHADER_H_
#define O3D_CORE_CROSS_CANVAS_SHADER_H_

#include <vector>

#include "core/cross/param_object.h"
#include "core/cross/param.h"

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPoint.h"

class SkShader;

namespace o3d {

// This is the base class for 2D gradient shaders that can be applied to a
// CanvasPaint.  The shaders affect both text and primitive drawing in the
// canvas.
class CanvasShader : public ParamObject {
 public:
  enum TileMode {
    CLAMP,  /* copy the edge color if the shader draws outside of its bounds */
    REPEAT,  /* repeat horizontally and vertically outside its bounds */
    MIRROR  /* same, alternating mirror images */
  };
  typedef SmartPointer<CanvasShader> Ref;

  explicit CanvasShader(ServiceLocator* service_locator);
  virtual ~CanvasShader();

  // Returns a pointer to the native (SKIA) shader object.
  SkShader* GetNativeShader();

 protected:
  // Creates a new native (SKIA) shader object using the parameter values
  // stored in the object.
  virtual SkShader* MakeNativeShader() = 0;
  SkShader* native_shader_;
  bool needs_update_;

 private:
  O3D_DECL_CLASS(CanvasShader, ParamObject)
  DISALLOW_COPY_AND_ASSIGN(CanvasShader);
};

// A shader that generates a linear gradient between two specified points.
// Two or more colors need to be specified for the gradient.
class CanvasLinearGradient : public CanvasShader {
 public:
  typedef SmartPointer<CanvasLinearGradient> Ref;
  virtual ~CanvasLinearGradient();

  // Sets the start point of the gradient.
  void set_start_point(Float2 start_point) {
    start_point_ = start_point;
  }

  // Returns the start point of the gradient.
  Float2 start_point() {
    return start_point_;
  }

  // Sets the end point of the gradient.
  void set_end_point(Float2 end_point) {
    end_point_ = end_point;
  }

  // Returns the end point of the gradient.
  Float2 end_point() {
    return end_point_;
  }

  // Sets the relative positions corresponding to the colors in the color array.
  // If the positions list is empty then the colors will be spread evenly.
  // Otherwise, position values should start with 0 and end with 1.0 and their
  // count should be equal to the length of the colors_ array.
  void set_positions(std::vector<float> positions) {
    positions_ = positions;
    needs_update_ = true;
  }

  std::vector<float> positions() {
    return positions_;
  }

  // Sets the array of colors used by the gradient.
  void set_colors(std::vector<Float4> colors) {
    colors_ = colors;
    needs_update_ = true;
  }

  // Returns the color array.
  std::vector<Float4> colors() {
    return colors_;
  }

  // Sets the tiling mode for the gradient which specifies how the shader
  // repeats for values beyond the start and end points.
  void set_tile_mode(CanvasShader::TileMode tile_mode) {
    tile_mode_ = tile_mode;
    needs_update_ = true;
  }

  // Returns the tile mode.
  TileMode tile_mode() { return tile_mode_; }

 protected:
  explicit CanvasLinearGradient(ServiceLocator* service_locator);
  virtual SkShader* MakeNativeShader();
 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // Start and end points for the gradient.
  Float2 start_point_;
  Float2 end_point_;

  // Array of colors.
  std::vector<Float4> colors_;

  // Array of positions corresponding to colors.
  std::vector<float> positions_;

  CanvasShader::TileMode tile_mode_;

  O3D_DECL_CLASS(CanvasLinearGradient, CanvasShader)
  DISALLOW_COPY_AND_ASSIGN(CanvasLinearGradient);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_CANVAS_SHADER_H_
