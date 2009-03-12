// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/separator.h"

#include "chrome/views/hwnd_view.h"

namespace views {

static const int kSeparatorSize = 2;

Separator::Separator() {
  SetFocusable(false);
}

Separator::~Separator() {
}

HWND Separator::CreateNativeControl(HWND parent_container) {
  SetFixedHeight(kSeparatorSize, CENTER);

  return ::CreateWindowEx(GetAdditionalExStyle(), L"STATIC", L"",
                          WS_CHILD | SS_ETCHEDHORZ | SS_SUNKEN,
                          0, 0, width(), height(),
                          parent_container, NULL, NULL, NULL);
}

LRESULT Separator::OnNotify(int w_param, LPNMHDR l_param) {
  return 0;
}

gfx::Size Separator::GetPreferredSize() {
  return gfx::Size(width(), fixed_height_);
}

}  // namespace views
