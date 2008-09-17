// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/star_toggle.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/resource_bundle.h"

StarToggle::StarToggle(Delegate* delegate)
    : delegate_(delegate),
      state_(false),
      change_state_immediately_(false) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  state_off_ = rb.GetBitmapNamed(IDR_CONTENT_STAR_OFF);
  state_on_ = rb.GetBitmapNamed(IDR_CONTENT_STAR_ON);
  SetFocusable(true);
}

StarToggle::~StarToggle() {
}

void StarToggle::SetState(bool s) {
  if (s != state_) {
    state_ = s;
    SchedulePaint();
  }
}

bool StarToggle::GetState() const {
  return state_;
}

void StarToggle::Paint(ChromeCanvas* canvas) {
  PaintFocusBorder(canvas);
  canvas->DrawBitmapInt(state_ ? *state_on_ : *state_off_,
                        (width() - state_off_->width()) / 2,
                        (height() - state_off_->height()) / 2);
}

void StarToggle::GetPreferredSize(CSize* out) {
  out->cx = state_off_->width();
  out->cy = state_off_->height();
}

bool StarToggle::OnMouseDragged(const ChromeViews::MouseEvent& e) {
  return e.IsLeftMouseButton();
}

bool StarToggle::OnMousePressed(const ChromeViews::MouseEvent& e) {
  if (e.IsLeftMouseButton() && HitTest(WTL::CPoint(e.x(), e.y()))) {
    RequestFocus();
    return true;
  }
  return false;
}

void StarToggle::OnMouseReleased(const ChromeViews::MouseEvent& e,
                                 bool canceled) {
  if (e.IsLeftMouseButton() && HitTest(WTL::CPoint(e.x(), e.y())))
    SwitchState();
}

bool StarToggle::OnKeyPressed(const ChromeViews::KeyEvent& e) {
  if ((e.GetCharacter() == L' ') || (e.GetCharacter() == L'\n')) {
    SwitchState();
    return true;
  }
  return false;
}

void StarToggle::SwitchState() {
  const bool new_state = !state_;
  if (change_state_immediately_)
    state_ = new_state;
  SchedulePaint();
  delegate_->StarStateChanged(new_state);
}
