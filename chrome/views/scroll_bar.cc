// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/scroll_bar.h"

namespace views {

/////////////////////////////////////////////////////////////////////////////
//
// ScrollBar implementation
//
/////////////////////////////////////////////////////////////////////////////

ScrollBar::ScrollBar(bool is_horiz) : is_horiz_(is_horiz),
                                      controller_(NULL),
                                      max_pos_(0) {
}

ScrollBar::~ScrollBar() {
}

bool ScrollBar::IsHorizontal() const {
  return is_horiz_;
}

void ScrollBar::SetController(ScrollBarController* controller) {
  controller_ = controller;
}

ScrollBarController* ScrollBar::GetController() const {
  return controller_;
}

void ScrollBar::Update(int viewport_size, int content_size, int current_pos) {
  max_pos_ = std::max(0, content_size - viewport_size);
}

int ScrollBar::GetMaxPosition() const {
  return max_pos_;
}

int ScrollBar::GetMinPosition() const {
  return 0;
}

}  // namespace views
