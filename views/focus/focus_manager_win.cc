// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/focus/focus_manager.h"

#include "views/view.h"
#include "views/widget/widget_win.h"

namespace views {

void FocusManager::ClearNativeFocus() {
  // Keep the top root window focused so we get keyboard events.
  ::SetFocus(widget_->GetNativeView());
}

void FocusManager::FocusNativeView(gfx::NativeView native_view) {
  // Only reset focus if hwnd is not already focused.
  if (native_view && ::GetFocus() != native_view)
    ::SetFocus(native_view);
}

// static
FocusManager* FocusManager::GetFocusManagerForNativeView(
    gfx::NativeView native_view) {
  HWND root = ::GetAncestor(native_view, GA_ROOT);
  if (!root)
    return NULL;
  WidgetWin* widget = WidgetWin::GetWidget(root);
  return widget ? widget->GetFocusManager() : NULL;
}

}  // namespace views

