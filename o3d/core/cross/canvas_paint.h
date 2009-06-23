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


// This file contains the declaration for the Paint class.

#ifndef O3D_CORE_CROSS_CANVAS_PAINT_H_
#define O3D_CORE_CROSS_CANVAS_PAINT_H_

#include "core/cross/param_object.h"
#include "core/cross/param.h"
#include "core/cross/canvas_shader.h"

#include "third_party/skia/include/core/SkPaint.h"

namespace o3d {

class Float4;

// Class containing the metrics describing properties of a font used by the
// paint object.
class CanvasFontMetrics {
 public:
  float top() const { return top_; }
  void set_top(float top) { top_ = top; }
  float ascent() const { return ascent_; }
  void set_ascent(float ascent) { ascent_ = ascent; }
  float descent() const { return descent_; }
  void set_descent(float descent) { descent_ = descent; }
  float bottom() const { return bottom_; }
  void set_bottom(float bottom) { bottom_ = bottom; }
  float leading() const { return leading_; }
  void set_leading(float leading) { leading_ = leading; }

 private:
  // The greatest distance above the baseline for any glyph (will be <= 0)
  float top_;
  // The recommended distance above the baseline (will be <= 0)
  float ascent_;
  // The recommended distance below the baseline (will be >= 0)
  float descent_;
  // The greatest distance below the baseline for any glyph (will be >= 0)
  float bottom_;
  // The recommended distance to add between lines of text (will be >= 0)
  float leading_;
};


// The Paint class is used for specifying how to draw to
// a canvas
class CanvasPaint : public ParamObject {
 public:
  enum Style {
    NORMAL,
    BOLD,
    ITALIC,
    BOLD_ITALIC
  };

  enum TextAlign {
    LEFT,
    CENTER,
    RIGHT
  };

  typedef SmartPointer<CanvasPaint> Ref;

  virtual ~CanvasPaint();

  // Returns the color used for the drawing operations using this paint.
  Float4 color() { return color_; }

  // Sets the color.
  void set_color(const Float4& color) {
    color_ = color;
    needs_update_ = true;
  }

  // Sets the color and radius of an outline around the text.  Setting the
  // radius to 0 cancels the outline effect.
  // Parameters:
  //   radius: The radius of the outline effect in pixels.
  //   color: The color used for outlining (in RGBA)
  void SetOutline(float radius, const Float4& color) {
    outline_radius_ = radius;
    outline_color_ = color;
    if (outline_radius_ != 0)
      shadow_radius_ = 0;
    needs_update_ = true;
  }

  // Sets a shadow effect around text.  Setting the radius to 0 cancels the
  // shadow effect.
  // Parameters:
  //   radius: The radius of shadow effect in pixels
  //   offset_x: The offset of the shadow in the x coordinate.
  //   offset_y: The offset of the shadow in the y coordinate.
  //   color: The color used for the shadow (in RGBA)
  void SetShadow(float radius,
                 float offset_x,
                 float offset_y,
                 const Float4& color) {
    shadow_radius_ = radius;
    shadow_offset_x_ = offset_x;
    shadow_offset_y_ = offset_y;
    shadow_color_ = color;
    if (shadow_radius_ != 0) {
      outline_radius_ = 0;
    }
    needs_update_ = true;
  }

  // Returns the font metrics for the current font being used by the paint.
  CanvasFontMetrics GetFontMetrics();

  // Returns the bounds of the given text string when rendered with this paint.
  // The bounds are returned as a Float4 containing [left, top, right, bottom]
  // values relative to (0, 0).
  Float4 MeasureText(const String& text);

  // Sets the size of the font used for text drawing.
  // Parameters:
  //   text_size: The size of the text
  void set_text_size(float text_size) {
    text_size_ = text_size;
    needs_update_ = true;
  }

  // Returns the current font size used for drawing text.
  float text_size() { return text_size_; }

  // Sets the typeface of the font used for rendering text.
  // Parameters:
  //   font: typeface to use.
  void set_text_typeface(const String& typeface) {
    text_typeface_ = typeface;
    needs_update_ = true;
  }

  // Returns the typeface used for text drawing.
  String text_typeface() {
    return text_typeface_;
  }

  // Sets the style parameter for the text (e.g. italic, bold, etc)
  void set_text_style(Style style) {
    text_style_ = style;
    needs_update_ = true;
  }

  // Returns the style used for drawing text.
  Style text_style() {
    return text_style_;
  }

  // Sets the pointer to the CanvasShader object used by this paint.  The
  // CanvasShader specifies in detail how paint gets applied (e.g. with a
  // linear gradient, etc)
  void set_shader(CanvasShader* shader) {
    shader_ = CanvasShader::Ref(shader);
    needs_update_ = true;
  }

  // Returns the current shader used by the paint.
  CanvasShader* shader() {
    return shader_.Get();
  }

  // Sets the alignment mode for renderered text.  This is the setting that's
  // used by the canvas text drawing methods.
  void set_text_align(TextAlign text_align) {
    text_align_ = text_align;
    needs_update_ = true;
  }

  // Returns the text alingment setting.
  TextAlign text_align() { return text_align_; }

 protected:
  explicit CanvasPaint(ServiceLocator* service_locator);

 private:
  friend class Canvas;
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // Updates the internal skia paint object using the current parameter values.
  void UpdateNativePaint();

  const SkPaint& GetNativePaint() {
    UpdateNativePaint();
    return sk_paint_;
  }

  // CanvasShader object used by this paint.
  CanvasShader::Ref shader_;

  // Skia paint object.
  SkPaint sk_paint_;

  // This flag is set to true when one of the paint parameters is changed.  When
  // the GetNativePaint() method is called, if this flag is set it will force
  // a rebuild of the native SkPaint object.
  bool needs_update_;

  // Alignment mode used for drawing text.
  TextAlign text_align_;

  // Color used for text and drawing operations (RGBA)
  Float4 color_;

  // Size of font used for drawing text.
  float text_size_;

  // Typeface of font used for text drawing.
  String text_typeface_;

  // Style of text used for rendering (e.g. bold, italic, etc)
  Style text_style_;

  // Radius (in pixels) of outline around text
  float outline_radius_;

  // Color used for outlining text.
  Float4 outline_color_;

  // Parameters for shadow effect around text.
  float shadow_radius_;
  float shadow_offset_x_;
  float shadow_offset_y_;
  Float4 shadow_color_;

  O3D_DECL_CLASS(CanvasPaint, ParamObject)
  DISALLOW_COPY_AND_ASSIGN(CanvasPaint);
};


}  // namespace o3d

#endif  // O3D_CORE_CROSS_CANVAS_PAINT_H_
