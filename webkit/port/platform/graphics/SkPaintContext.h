// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SkPaintContext_h
#define SkPaintContext_h

#include "SkDashPathEffect.h"
#include "SkDrawLooper.h"
#include "SkDeque.h"
#include "SkPaint.h"
#include "SkPath.h"

namespace gfx {
class PlatformCanvasWin;
}

// This class is the interface to communicate to Skia. It is meant to be as
// opaque as possible. This class accept Skia native data format and not WebKit
// format.
// Every functions assume painting is enabled, callers should check this before
// calling any member function.
class SkPaintContext {
 public:
  // Duplicates WebCore::StrokeStyle. We can't include GraphicsContext.h here
  // because we want to keep this class isolated from WebKit. Make sure both
  // enums keep the same values. If StrokeStyle ever gets moved to
  // GraphicsTypes.h, remove this duplicate and include GraphicsTypes.h
  // instead.
  enum StrokeStyle {
    NoStroke,
    SolidStroke,
    DottedStroke,
    DashedStroke
  };

  // Context will be NULL if painting should be disabled.
  SkPaintContext(gfx::PlatformCanvasWin* context);
  ~SkPaintContext();

  void save();

  void restore();

  void drawRect(SkRect rect);

  void setup_paint_common(SkPaint* paint) const;

  void setup_paint_fill(SkPaint* paint) const;

  // Sets up the paint for stroking. Returns an int representing the width of
  // the pen, or 1 if the pen's width is 0 if a non-zero length is provided,
  // the number of dashes/dots on a dashed/dotted line will be adjusted to
  // start and end that length with a dash/dot.
  int setup_paint_stroke(SkPaint* paint, SkRect* rect, int length);

  // State proxying functions
  SkDrawLooper* setDrawLooper(SkDrawLooper* dl);
  void setMiterLimit(float ml);
  void setAlpha(float alpha);
  void setLineCap(SkPaint::Cap lc);
  void setLineJoin(SkPaint::Join lj);
  void setFillRule(SkPath::FillType fr);
  void setPorterDuffMode(SkPorterDuff::Mode pdm);
  void setFillColor(SkColor color);
  void setStrokeStyle(StrokeStyle strokestyle);
  void setStrokeColor(SkColor strokecolor);
  void setStrokeThickness(float thickness);

  void beginPath();
  void addPath(const SkPath& path);
  SkPath* currentPath();

  void setGradient(SkShader*);
  void setPattern(SkShader*);
  void setDashPathEffect(SkDashPathEffect*);

  SkColor fillColor() const;

 protected:
  gfx::PlatformCanvasWin* canvas() {
    return canvas_;
  }

 private:
  // Defines drawing style.
  struct State;

  // NULL indicates painting is disabled. Never delete this object.
  gfx::PlatformCanvasWin* canvas_;

  // States stack. Enables local drawing state change with save()/restore()
  // calls.
  SkDeque state_stack_;
  // Pointer to the current drawing state. This is a cached value of
  // mStateStack.back().
  State* state_;

  // Current path
  SkPath path_;

  // Disallow these.
  SkPaintContext(const SkPaintContext&);
  void operator=(const SkPaintContext&);
};

#endif
