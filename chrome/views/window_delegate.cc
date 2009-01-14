// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/window_delegate.h"

// TODO(beng): hrmp. Fix this in http://crbug.com/4406
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_service.h"
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

void WindowDelegate::SaveWindowPlacement(const gfx::Rect& bounds,
                                         bool maximized,
                                         bool always_on_top) {
  std::wstring window_name = GetWindowName();
  if (window_name.empty() || !g_browser_process->local_state())
    return;

  DictionaryValue* window_preferences =
      g_browser_process->local_state()->GetMutableDictionary(
          window_name.c_str());
  window_preferences->SetInteger(L"left", bounds.x());
  window_preferences->SetInteger(L"top", bounds.y());
  window_preferences->SetInteger(L"right", bounds.right());
  window_preferences->SetInteger(L"bottom", bounds.bottom());
  window_preferences->SetBoolean(L"maximized", maximized);
  window_preferences->SetBoolean(L"always_on_top", always_on_top);
}

bool WindowDelegate::GetSavedWindowBounds(gfx::Rect* bounds) const {
  std::wstring window_name = GetWindowName();
  if (window_name.empty())
    return false;

  const DictionaryValue* dictionary = 
      g_browser_process->local_state()->GetDictionary(window_name.c_str());
  int left, top, right, bottom;
  if (!dictionary || !dictionary->GetInteger(L"left", &left) ||
      !dictionary->GetInteger(L"top", &top) ||
      !dictionary->GetInteger(L"right", &right) ||
      !dictionary->GetInteger(L"bottom", &bottom))
    return false;

  bounds->SetRect(left, top, right - left, bottom - top);
  return true;
}

bool WindowDelegate::GetSavedMaximizedState(bool* maximized) const {
  std::wstring window_name = GetWindowName();
  if (window_name.empty())
    return false;

  const DictionaryValue* dictionary = 
      g_browser_process->local_state()->GetDictionary(window_name.c_str());
  return dictionary && dictionary->GetBoolean(L"maximized", maximized);
}

bool WindowDelegate::GetSavedAlwaysOnTopState(bool* always_on_top) const {
  if (!g_browser_process->local_state())
    return false;

  std::wstring window_name = GetWindowName();
  if (window_name.empty())
    return false;

  const DictionaryValue* dictionary = 
      g_browser_process->local_state()->GetDictionary(window_name.c_str());
  return dictionary && dictionary->GetBoolean(L"always_on_top", always_on_top);
}


ClientView* WindowDelegate::CreateClientView(Window* window) {
  return new ClientView(window, GetContentsView());
}

void WindowDelegate::ReleaseWindow() {
  window_.release();
}

}  // namespace views

