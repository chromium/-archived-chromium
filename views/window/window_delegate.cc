// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/window_delegate.h"
#include "views/views_delegate.h"
#include "views/window/client_view.h"
#include "views/window/window.h"
#include "third_party/skia/include/core/SkBitmap.h"

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

void WindowDelegate::SaveWindowPlacement(const gfx::Rect& bounds,
                                         bool maximized,
                                         bool always_on_top) {
  std::wstring window_name = GetWindowName();
  if (!ViewsDelegate::views_delegate || window_name.empty())
    return;

  ViewsDelegate::views_delegate->SaveWindowPlacement(
      window_name, bounds, maximized, always_on_top);
}

bool WindowDelegate::GetSavedWindowBounds(gfx::Rect* bounds) const {
  std::wstring window_name = GetWindowName();
  if (!ViewsDelegate::views_delegate || window_name.empty())
    return false;

  return ViewsDelegate::views_delegate->GetSavedWindowBounds(
      window_name, bounds);
}

bool WindowDelegate::GetSavedMaximizedState(bool* maximized) const {
  std::wstring window_name = GetWindowName();
  if (!ViewsDelegate::views_delegate || window_name.empty())
    return false;

  return ViewsDelegate::views_delegate->GetSavedMaximizedState(
      window_name, maximized);
}

bool WindowDelegate::GetSavedAlwaysOnTopState(bool* always_on_top) const {
  std::wstring window_name = GetWindowName();
  if (!ViewsDelegate::views_delegate || window_name.empty())
    return false;

  return ViewsDelegate::views_delegate->GetSavedAlwaysOnTopState(
      window_name, always_on_top);
}


ClientView* WindowDelegate::CreateClientView(Window* window) {
  return new ClientView(window, GetContentsView());
}

void WindowDelegate::ReleaseWindow() {
  window_.release();
}

}  // namespace views
