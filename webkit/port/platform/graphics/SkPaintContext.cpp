// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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

#include "config.h"

#include "SkPaintContext.h"

#include "SkColorPriv.h"
#include "SkShader.h"
#include "SkDashPathEffect.h"

namespace {

int RoundToInt(float x) {
  // Android uses roundf which VC doesn't have, emulate that function.
  if (fmodf(x, 1.0f) >= 0.5f)
    return (int)ceilf(x);
  return (int)floorf(x);
}

}

/*  TODO / questions

    mAlpha: how does this interact with the alpha in Color? multiply them together?
    mPorterDuffMode: do I always respect this? If so, then
                     the rgb() & 0xFF000000 check will abort drawing too often
    Is Color premultiplied or not? If it is, then I can't blindly pass it to paint.setColor()
*/
struct SkPaintContext::State {
  float               mMiterLimit;
  float               mAlpha;
  SkDrawLooper*       mLooper;
  SkPaint::Cap        mLineCap;
  SkPaint::Join       mLineJoin;
  SkPorterDuff::Mode  mPorterDuffMode;
  // Ratio of the length of a dash to its width.
  int                 mDashRatio;
  SkColor             mFillColor;
  StrokeStyle         mStrokeStyle;
  SkColor             mStrokeColor;
  float               mStrokeThickness;
  bool                mUseAntialiasing;

  SkDashPathEffect*   mDash;
  SkShader*           mGradient;
  SkShader*           mPattern;

  // Note: Keep theses default values in sync with GraphicsContextState.
  State()
      : mMiterLimit(4),
        mAlpha(1),
        mLooper(NULL),
        mLineCap(SkPaint::kDefault_Cap),
        mLineJoin(SkPaint::kDefault_Join),
        mPorterDuffMode(SkPorterDuff::kSrcOver_Mode),
        mDashRatio(3),
        mFillColor(0xFF000000),
        mStrokeStyle(SolidStroke),
        mStrokeColor(0x0FF000000),
        mStrokeThickness(0),
        mUseAntialiasing(true),
        mDash(NULL),
        mGradient(NULL),
        mPattern(NULL) {
  }

  State(const State& other) {
    other.mLooper->safeRef();
    memcpy(this, &other, sizeof(State));

    mDash->safeRef();
    mGradient->safeRef();
    mPattern->safeRef();
  }

  ~State() {
    mLooper->safeUnref();
    mDash->safeUnref();
    mGradient->safeUnref();
    mPattern->safeUnref();
  }

  SkDrawLooper* setDrawLooper(SkDrawLooper* dl) {
    SkRefCnt_SafeAssign(mLooper, dl);
    return dl;
  }

  SkColor applyAlpha(SkColor c) const {
    int s = RoundToInt(mAlpha * 256);
    if (s >= 256)
      return c;           
    if (s < 0)
      return 0;

    int a = SkAlphaMul(SkColorGetA(c), s);
    return (c & 0x00FFFFFF) | (a << 24);
  }

 private:
  // Not supported yet.
  void operator=(const State&);
};

// Context will be NULL if painting should be disabled.
SkPaintContext::SkPaintContext(gfx::PlatformCanvas* context)
      : canvas_(context),
        state_stack_(sizeof(State)) {
  State* state = reinterpret_cast<State*>(state_stack_.push_back());
  new (state) State();
  state_ = state;
}

SkPaintContext::~SkPaintContext() {
  // we force restores so we don't leak any subobjects owned by our
  // stack of State records.
  while (state_stack_.count() > 0) {
    reinterpret_cast<State*>(state_stack_.back())->~State();
    state_stack_.pop_back();
  }
}

void SkPaintContext::save() {
  State* newState = reinterpret_cast<State*>(state_stack_.push_back());
  new (newState) State(*state_);
  state_ = newState;

  // Save our native canvas.
  canvas()->save();
}

void SkPaintContext::restore() {
  // Restore our native canvas.
  canvas()->restore();

  state_->~State();
  state_stack_.pop_back();

  state_ = reinterpret_cast<State*>(state_stack_.back());
}

void SkPaintContext::drawRect(SkRect rect) {
  SkPaint paint;
  int fillcolor_not_transparent = state_->mFillColor & 0xFF000000;
  if (fillcolor_not_transparent) {
    setup_paint_fill(&paint);
    canvas()->drawRect(rect, paint);
  }

  if (state_->mStrokeStyle != NoStroke && (state_->mStrokeColor & 0xFF000000)) {
    if (fillcolor_not_transparent) {
      // This call is expensive so don't call it unnecessarily.
      paint.reset();
    }
    setup_paint_stroke(&paint, &rect, 0);
    canvas()->drawRect(rect, paint);
  }
}

void SkPaintContext::setup_paint_common(SkPaint* paint) const {
#ifdef SK_DEBUGx
  {
    SkPaint defaultPaint;
    SkASSERT(*paint == defaultPaint);
  }
#endif

  paint->setAntiAlias(state_->mUseAntialiasing);
  paint->setPorterDuffXfermode(state_->mPorterDuffMode);
  paint->setLooper(state_->mLooper);

  if(state_->mGradient) {
    paint->setShader(state_->mGradient);
  } else if (state_->mPattern) {
    paint->setShader(state_->mPattern);
  }
}

void SkPaintContext::setup_paint_fill(SkPaint* paint) const {
  setup_paint_common(paint);
  paint->setColor(state_->applyAlpha(state_->mFillColor));
}

int SkPaintContext::setup_paint_stroke(SkPaint* paint,
                                       SkRect* rect,
                                       int length) const {
  setup_paint_common(paint);
  float width = state_->mStrokeThickness;

  //this allows dashing and dotting to work properly for hairline strokes
  if (0 == width)
    width = 1;

  paint->setColor(state_->applyAlpha(state_->mStrokeColor));
  paint->setStyle(SkPaint::kStroke_Style);
  paint->setStrokeWidth(SkFloatToScalar(width));
  paint->setStrokeCap(state_->mLineCap);
  paint->setStrokeJoin(state_->mLineJoin);
  paint->setStrokeMiter(SkFloatToScalar(state_->mMiterLimit));

  if (rect != NULL && (RoundToInt(width) & 1)) {
    rect->inset(-SK_ScalarHalf, -SK_ScalarHalf);
  }

  if (state_->mDash) {
    paint->setPathEffect(state_->mDash);
  } else {
    switch (state_->mStrokeStyle) {
    case NoStroke:
    case SolidStroke:
      break;
    case DashedStroke:
      width = state_->mDashRatio * width;
      /* no break */
    case DottedStroke:
      SkScalar dashLength;
      if (length) {
        //determine about how many dashes or dots we should have
        int numDashes = length/RoundToInt(width);
        if (!(numDashes & 1))
          numDashes++;    //make it odd so we end on a dash/dot
        //use the number of dashes to determine the length of a dash/dot, which will be approximately width
        dashLength = SkScalarDiv(SkIntToScalar(length), SkIntToScalar(numDashes));
      } else {
        dashLength = SkFloatToScalar(width);
      }
      SkScalar intervals[2] = { dashLength, dashLength };
      paint->setPathEffect(new SkDashPathEffect(intervals, 2, 0))->unref();
    }
  }
  return RoundToInt(width);
}

SkDrawLooper* SkPaintContext::setDrawLooper(SkDrawLooper* dl) {
  return state_->setDrawLooper(dl);
}

void SkPaintContext::setMiterLimit(float ml) {
  state_->mMiterLimit = ml;
}

void SkPaintContext::setAlpha(float alpha) {
  state_->mAlpha = alpha;
}

void SkPaintContext::setLineCap(SkPaint::Cap lc) {
  state_->mLineCap = lc;
}

void SkPaintContext::setLineJoin(SkPaint::Join lj) {
  state_->mLineJoin = lj;
}

void SkPaintContext::setPorterDuffMode(SkPorterDuff::Mode pdm) {
  state_->mPorterDuffMode = pdm;
}

void SkPaintContext::setFillColor(SkColor color) {
  state_->mFillColor = color;
}

void SkPaintContext::setStrokeStyle(StrokeStyle strokestyle) {
  state_->mStrokeStyle = strokestyle;
}

void SkPaintContext::setStrokeColor(SkColor strokecolor) {
  state_->mStrokeColor = strokecolor;
}

void SkPaintContext::setStrokeThickness(float thickness) {
  state_->mStrokeThickness = thickness;
}

void SkPaintContext::setUseAntialiasing(bool enable) {
  state_->mUseAntialiasing = enable;
}

SkColor SkPaintContext::fillColor() const {
  return state_->mFillColor;
}

void SkPaintContext::beginPath() {
  path_.reset();
}

void SkPaintContext::addPath(const SkPath& path) {
  path_.addPath(path);
}

const SkPath* SkPaintContext::currentPath() const {
  return &path_;
}

void SkPaintContext::setFillRule(SkPath::FillType fr) {
  path_.setFillType(fr);
}

void SkPaintContext::setGradient(SkShader* gradient) {
  if (gradient != state_->mGradient) {
    state_->mGradient->safeUnref();
    state_->mGradient=gradient;
  }
}

void SkPaintContext::setPattern(SkShader* pattern) {
  if (pattern != state_->mPattern) {
    state_->mPattern->safeUnref();
    state_->mPattern=pattern;
  }
}

void SkPaintContext::setDashPathEffect(SkDashPathEffect* dash) {
  if (dash != state_->mDash) {
    state_->mDash->safeUnref();
    state_->mDash=dash;
  }
}

