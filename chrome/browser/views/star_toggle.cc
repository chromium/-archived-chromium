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
                        (GetWidth() - state_off_->width()) / 2,
                        (GetHeight() - state_off_->height()) / 2);
}

void StarToggle::GetPreferredSize(CSize* out) {
  out->cx = state_off_->width();
  out->cy = state_off_->height();
}

bool StarToggle::OnMouseDragged(const ChromeViews::MouseEvent& e) {
  return e.IsLeftMouseButton();
}

bool StarToggle::OnMousePressed(const ChromeViews::MouseEvent& e) {
  if (e.IsLeftMouseButton() && HitTest(e.GetLocation())) {
    RequestFocus();
    return true;
  }
  return false;
}

void StarToggle::OnMouseReleased(const ChromeViews::MouseEvent& e,
                                 bool canceled) {
  if (e.IsLeftMouseButton() && HitTest(e.GetLocation()))
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