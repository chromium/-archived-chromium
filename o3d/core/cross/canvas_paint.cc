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


// This file contains the implementation for the CanvasPaint class.

#include "core/cross/precompile.h"
#include "core/cross/canvas_paint.h"
#include "core/cross/canvas_utils.h"
#include "core/cross/client.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkBlurDrawLooper.h"

namespace o3d {

namespace {

/** StrokeDrawLooper This class draws an outline of the
*   object, and then draws the original object in its original
*   position.
*/
class StrokeDrawLooper : public SkDrawLooper {
 public:
  StrokeDrawLooper(SkScalar radius, SkColor color);
  virtual ~StrokeDrawLooper() {}
  virtual void init(SkCanvas* canvas, SkPaint* paint);
  virtual bool next();
  virtual void restore();

 protected:
  virtual Factory getFactory() { return CreateProc; }
  void flatten(SkFlattenableWriteBuffer& buffer);
  StrokeDrawLooper(SkFlattenableReadBuffer& buffer);

 private:
  SkCanvas* fCanvas;
  SkPaint* fPaint;

  // These are to save the state attributes that we want to change so
  // we can restore them after we draw the stroke.
  SkPaint::Style fSavedStyle;
  SkScalar fSavedStrokeWidth;
  SkColor fSavedColor;

  // These are the attribues of the stroke.
  SkScalar fRadius;
  SkColor fColor;

  // Possible machine states for this object.
  enum State {
    kBeforeEdge,
    kAfterEdge,
    kDone,
  };
  State   fState;

  // Factory method for ressurecting a StrokeDrawLooper.
  static SkFlattenable* CreateProc(SkFlattenableReadBuffer& buffer) {
        return SkNEW_ARGS(StrokeDrawLooper, (buffer)); }

  typedef SkDrawLooper INHERITED;
};

StrokeDrawLooper::StrokeDrawLooper(SkScalar radius, SkColor color)
    : fColor(color), fRadius(radius) {
}

void StrokeDrawLooper::init(SkCanvas* canvas, SkPaint* paint) {
  fState = kBeforeEdge;
  fCanvas = canvas;
  fPaint = paint;
}

bool StrokeDrawLooper::next() {
  switch (fState) {
    case kBeforeEdge:
      // Save the original values.
      fSavedStyle = fPaint->getStyle();
      fSavedStrokeWidth = fPaint->getStrokeWidth();
      fSavedColor = fPaint->getColor();

      // Override with stroke values.
      fPaint->setColor(fColor);
      fPaint->setStrokeWidth(fRadius);
      fPaint->setStyle(SkPaint::kStroke_Style);

      // Change states.
      fState = kAfterEdge;
      return true;
    case kAfterEdge:
      // Restore original values.
      fPaint->setColor(fSavedColor);
      fPaint->setStrokeWidth(fSavedStrokeWidth);
      fPaint->setStyle(fSavedStyle);

      // Now we're done.
      fState = kDone;
      return true;
    default:
      SkASSERT(kDone == fState);
      return false;
  }
}

void StrokeDrawLooper::restore() {
  if (kAfterEdge == fState) {
    fPaint->setColor(fSavedColor);
    fPaint->setStrokeWidth(fSavedStrokeWidth);
    fPaint->setStyle(fSavedStyle);
    fState = kDone;
  }
}

void StrokeDrawLooper::flatten(SkFlattenableWriteBuffer& buffer) {
  buffer.writeScalar(fRadius);
  buffer.write32(fColor);
}

StrokeDrawLooper::StrokeDrawLooper(SkFlattenableReadBuffer& buffer) {
  fRadius = buffer.readScalar();
  fColor = buffer.readU32();
}

}  // end anonymous namespace

O3D_DEFN_CLASS(CanvasPaint, ParamObject);

static SkPaint::Align ToSKAlign(CanvasPaint::TextAlign align) {
  switch (align) {
    case CanvasPaint::LEFT:
      return SkPaint::kLeft_Align;
    case CanvasPaint::CENTER:
      return SkPaint::kCenter_Align;
    case CanvasPaint::RIGHT:
      return SkPaint::kRight_Align;
    default:
      return SkPaint::kLeft_Align;
  }
}

CanvasPaint::CanvasPaint(ServiceLocator* service_locator)
    : ParamObject(service_locator),
      shader_(NULL),
      needs_update_(true),
      color_(Float4(0, 0, 0, 1)),
      text_align_(LEFT),
      text_size_(10),
      shadow_radius_(0),
      shadow_color_(Float4(0, 0, 0, 1)),
      shadow_offset_x_(0),
      shadow_offset_y_(0),
      outline_radius_(0),
      outline_color_(Float4(0, 0, 0, 1)),
      text_style_(NORMAL),
      text_typeface_("") {
  sk_paint_.setAntiAlias(true);
}

CanvasPaint::~CanvasPaint() {
}

// Updates the native paint object with the current values of all the settings.
void CanvasPaint::UpdateNativePaint() {
  if (needs_update_) {
    sk_paint_.setColor(Float4ToSkColor(color_));

    // Note that shadow and ouline cannot both be active at the same time.
    if (outline_radius_ != 0.0) {
      SkDrawLooper* l = new StrokeDrawLooper(SkFloatToScalar(outline_radius_),
                                             Float4ToSkColor(outline_color_));
      sk_paint_.setLooper(l);
      l->unref();
    } else if (shadow_radius_ != 0.0) {
      SkBlurDrawLooper* l = new  SkBlurDrawLooper(
          SkFloatToScalar(shadow_radius_),
          SkFloatToScalar(shadow_offset_x_),
          SkFloatToScalar(shadow_offset_y_),
          Float4ToSkColor(shadow_color_));
      sk_paint_.setLooper(l);
      l->unref();
    } else {
      sk_paint_.setLooper(NULL);
    }

#ifndef OS_LINUX
    sk_paint_.setTextSize(SkFloatToScalar(text_size_));

    // TODO: Verify that there's visual parity between fonts on all
    // platforms.  If not, come up with a list of typefaces and their
    // correspondances across platforms.  The long term solution will be to
    // allow inclusion of Free Type fonts (Skia allows that) in the file format
    // so that we're guaranteed that text looks identical on all platforms.
    SkTypeface* tf = SkTypeface::CreateFromName(
        text_typeface_.c_str(),
        static_cast<SkTypeface::Style>(text_style_));
    sk_paint_.setTypeface(tf);
    tf->unref();
#endif

    if (shader_) {
      sk_paint_.setShader(shader_->GetNativeShader());
    } else {
      sk_paint_.setShader(NULL);
    }

#ifndef OS_LINUX
    sk_paint_.setTextAlign(ToSKAlign(text_align_));
#endif

    needs_update_ = false;
  }
}

CanvasFontMetrics CanvasPaint::GetFontMetrics() {
  CanvasFontMetrics ret;

#ifndef OS_LINUX
  SkPaint::FontMetrics metrics;
  GetNativePaint().getFontMetrics(&metrics);

  ret.set_ascent(metrics.fAscent);
  ret.set_bottom(metrics.fBottom);
  ret.set_descent(metrics.fDescent);
  ret.set_leading(metrics.fLeading);
  ret.set_top(metrics.fTop);
#else
  O3D_ERROR(service_locator()) << "Text is not yet supported on Linux";
  ret.set_top(0.f);
  ret.set_ascent(0.f);
  ret.set_descent(0.f);
  ret.set_bottom(0.f);
  ret.set_leading(0.f);
#endif

  return ret;
}

Float4 CanvasPaint::MeasureText(const String& text) {
#ifndef OS_LINUX
  SkRect bounds;
  GetNativePaint().measureText(text.c_str(), text.size(), &bounds);

  return Float4(bounds.fLeft, bounds.fTop, bounds.fRight, bounds.fBottom);
#else
  O3D_ERROR(service_locator()) << "Text is not yet supported on Linux";
  return Float4(0.f, 0.f, 0.f, 0.f);
#endif
}

ObjectBase::Ref CanvasPaint::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new CanvasPaint(service_locator));
}



}  // namespace o3d
