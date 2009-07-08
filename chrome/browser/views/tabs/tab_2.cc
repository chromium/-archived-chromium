// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/tabs/tab_2.h"

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/gfx/path.h"
#include "base/string_util.h"
#include "views/animator.h"

static const SkScalar kTabCapWidth = 15;
static const SkScalar kTabTopCurveWidth = 4;
static const SkScalar kTabBottomCurveWidth = 3;

////////////////////////////////////////////////////////////////////////////////
// Tab2, public:

Tab2::Tab2(Tab2Model* model)
    : model_(model),
      dragging_(false),
      removing_(false) {
}

Tab2::~Tab2() {
}

void Tab2::SetRemovingModel(Tab2Model* model) {
  removing_model_.reset(model);
  model_ = removing_model_.get();
}

bool Tab2::IsAnimating() const {
  return animator_.get() && animator_->IsAnimating();
}

views::Animator* Tab2::GetAnimator() {
  if (!animator_.get())
    animator_.reset(new views::Animator(this, model_->AsAnimatorDelegate()));
  return animator_.get();
}

// static
gfx::Size Tab2::GetStandardSize() {
  return gfx::Size(140, 27);
}

void Tab2::AddTabShapeToPath(gfx::Path* path) const {
  SkScalar h = SkIntToScalar(height());
  SkScalar w = SkIntToScalar(width());

  path->moveTo(0, h);

  // Left end cap.
  path->lineTo(kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(kTabCapWidth - kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(kTabCapWidth, 0);

  // Connect to the right cap.
  path->lineTo(w - kTabCapWidth, 0);

  // Right end cap.
  path->lineTo(w - kTabCapWidth + kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(w - kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(w, h);

  // Close out the path.
  path->lineTo(0, h);
  path->close();
}

////////////////////////////////////////////////////////////////////////////////
// Tab2, views::View overrides:

gfx::Size Tab2::GetPreferredSize() {
  return gfx::Size();
}

void Tab2::Layout() {
}

void Tab2::Paint(gfx::Canvas* canvas) {
  SkColor color = model_->IsSelected(this) ? SK_ColorRED : SK_ColorGREEN;

  gfx::Path path;
  AddTabShapeToPath(&path);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);
  canvas->drawPath(path, paint);

  // TODO(beng): less ad-hoc
  canvas->DrawStringInt(UTF16ToWideHack(model_->GetTitle(this)), gfx::Font(),
                        SK_ColorBLACK, 5, 3, 100, 20);
}

bool Tab2::OnMousePressed(const views::MouseEvent& event) {
  if (event.IsLeftMouseButton()) {
    model_->SelectTab(this);
    model_->CaptureDragInfo(this, event);
  }
  return true;
}

bool Tab2::OnMouseDragged(const views::MouseEvent& event) {
  dragging_ = true;
  return model_->DragTab(this, event);
}

void Tab2::OnMouseReleased(const views::MouseEvent& event, bool canceled) {
  if (dragging_) {
    dragging_ = false;
    model_->DragEnded(this);
  }
}

void Tab2::DidChangeBounds(const gfx::Rect& previous,
                           const gfx::Rect& current) {
}
