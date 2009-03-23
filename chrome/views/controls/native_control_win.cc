// #ifndef CHROME_VIEWS_NATIVE_CONTROL_WIN_H_// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/views/controls/native_control_win.h"

#include "base/logging.h"
#include "base/win_util.h"
#include "chrome/common/l10n_util_win.h"

namespace views {

// static
const wchar_t* NativeControlWin::kNativeControlWinKey =
    L"__NATIVE_CONTROL_WIN__";

static const wchar_t* kNativeControlOriginalWndProcKey =
    L"__NATIVE_CONTROL_ORIGINAL_WNDPROC__";

// static
WNDPROC NativeControlWin::original_wndproc_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// NativeControlWin, public:

NativeControlWin::NativeControlWin() : HWNDView() {
}

NativeControlWin::~NativeControlWin() {
}

LRESULT NativeControlWin::ProcessMessage(UINT message, WPARAM w_param,
                                         LPARAM l_param) {
  switch (message) {
    case WM_CONTEXTMENU:
      ShowContextMenu(gfx::Point(LOWORD(l_param), HIWORD(l_param)));
      break;
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
      return GetControlColor(message, reinterpret_cast<HDC>(w_param),
                             GetHWND());
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// NativeControlWin, View overrides:

void NativeControlWin::SetEnabled(bool enabled) {
  if (IsEnabled() != enabled) {
    View::SetEnabled(enabled);
    if (GetHWND())
      EnableWindow(GetHWND(), IsEnabled());
  }
}

void NativeControlWin::ViewHierarchyChanged(bool is_add, View* parent,
                                            View* child) {
  // Create the HWND when we're added to a valid Widget. Many controls need a
  // parent HWND to function properly.
  if (is_add && GetWidget() && !GetHWND())
    CreateNativeControl();

  // Call the base class to hide the view if we're being removed.
  HWNDView::ViewHierarchyChanged(is_add, parent, child);
}

void NativeControlWin::VisibilityChanged(View* starting_from, bool is_visible) {
  if (!is_visible) {
    // We destroy the child control HWND when we become invisible because of the
    // performance cost of maintaining many HWNDs.
    HWND hwnd = GetHWND();
    Detach();
    DestroyWindow(hwnd);
  } else if (!GetHWND()) {
    CreateNativeControl();
  }
}

void NativeControlWin::Focus() {
  DCHECK(GetHWND());
  SetFocus(GetHWND());
}

////////////////////////////////////////////////////////////////////////////////
// NativeControlWin, protected:

void NativeControlWin::ShowContextMenu(const gfx::Point& location) {
  if (!GetContextMenuController())
    return;

  int x = location.x();
  int y = location.y();
  bool is_mouse = true;
  if (x == -1 && y == -1) {
    gfx::Point point = GetKeyboardContextMenuLocation();
    x = point.x();
    y = point.y();
    is_mouse = false;
  }
  View::ShowContextMenu(x, y, is_mouse);
}

void NativeControlWin::NativeControlCreated(HWND native_control) {
  TRACK_HWND_CREATION(native_control);

  // Associate this object with the control's HWND so that WidgetWin can find
  // this object when it receives messages from it.
  SetProp(native_control, kNativeControlWinKey, this);

  // Subclass the window so we can monitor for key presses.
  original_wndproc_ =
      win_util::SetWindowProc(native_control,
                              &NativeControlWin::NativeControlWndProc);
  SetProp(native_control, kNativeControlOriginalWndProcKey, original_wndproc_);

  Attach(native_control);
  // GetHWND() is now valid.

  // Update the newly created HWND with any resident enabled state.
  EnableWindow(GetHWND(), IsEnabled());

  // This message ensures that the focus border is shown.
  SendMessage(GetHWND(), WM_CHANGEUISTATE,
              MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS), 0);
}

DWORD NativeControlWin::GetAdditionalExStyle() const {
  // If the UI for the view is mirrored, we should make sure we add the
  // extended window style for a right-to-left layout so the subclass creates
  // a mirrored HWND for the underlying control.
  DWORD ex_style = 0;
  if (UILayoutIsRightToLeft())
    ex_style |= l10n_util::GetExtendedStyles();

  return ex_style;
}

DWORD NativeControlWin::GetAdditionalRTLStyle() const {
  // If the UI for the view is mirrored, we should make sure we add the
  // extended window style for a right-to-left layout so the subclass creates
  // a mirrored HWND for the underlying control.
  DWORD ex_style = 0;
  if (UILayoutIsRightToLeft())
    ex_style |= l10n_util::GetExtendedTooltipStyles();

  return ex_style;
}

////////////////////////////////////////////////////////////////////////////////
// NativeControlWin, private:

LRESULT NativeControlWin::GetControlColor(UINT message, HDC dc, HWND sender) {
  View *ancestor = this;
  while (ancestor) {
    const Background* background = ancestor->background();
    if (background) {
      HBRUSH brush = background->GetNativeControlBrush();
      if (brush)
        return reinterpret_cast<LRESULT>(brush);
    }
    ancestor = ancestor->GetParent();
  }

  // COLOR_BTNFACE is the default for dialog box backgrounds.
  return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
}

// static
LRESULT NativeControlWin::NativeControlWndProc(HWND window,
                                               UINT message,
                                               WPARAM w_param,
                                               LPARAM l_param) {
  NativeControlWin* native_control =
      static_cast<NativeControlWin*>(GetProp(window, kNativeControlWinKey));
  DCHECK(native_control);

  if (message == WM_KEYDOWN && native_control->NotifyOnKeyDown()) {
    if (native_control->OnKeyDown(static_cast<int>(w_param)))
      return 0;
  } else if (message == WM_DESTROY) {
    win_util::SetWindowProc(window, native_control->original_wndproc_);
    RemoveProp(window, kNativeControlWinKey);
    TRACK_HWND_DESTRUCTION(window);
  }

  return CallWindowProc(native_control->original_wndproc_, window, message,
                        w_param, l_param);
}

}  // namespace views
