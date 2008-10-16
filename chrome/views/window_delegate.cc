// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/window_delegate.h"

#include "chrome/views/client_view.h"
#include "chrome/views/window.h"
#include "skia/include/SkBitmap.h"

namespace views {

WindowDelegate::WindowDelegate() {
}

WindowDelegate::~WindowDelegate() {
  ReleaseWindow();
}

// Returns the icon to be displayed in the window.
SkBitmap WindowDelegate::GetWindowIcon() {
  return SkBitmap();
}

ClientView* WindowDelegate::CreateClientView(Window* window) {
  return new ClientView(window, GetContentsView());
}

void WindowDelegate::ReleaseWindow() {
  window_.release();
}

}  // namespace views

