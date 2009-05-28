// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/hwnd_view.h"

#include "app/gfx/canvas.h"
#include "base/logging.h"
#include "views/focus/focus_manager.h"
#include "views/widget/widget.h"

namespace views {

static const char kViewClassName[] = "views/HWNDView";

HWNDView::HWNDView() : NativeViewHost() {
}

HWNDView::~HWNDView() {
}

void HWNDView::Attach(HWND hwnd) {
  DCHECK(native_view() == NULL);
  DCHECK(hwnd) << "Impossible detatched tab case; See crbug.com/6316";

  set_native_view(hwnd);

  // First hide the new window. We don't want anything to draw (like sub-hwnd
  // borders), when we change the parent below.
  ShowWindow(hwnd, SW_HIDE);

  // Need to set the HWND's parent before changing its size to avoid flashing.
  ::SetParent(hwnd, GetWidget()->GetNativeView());
  Layout();

  // Register with the focus manager so the associated view is focused when the
  // native control gets the focus.
  FocusManager::InstallFocusSubclass(
      hwnd, associated_focus_view() ? associated_focus_view() : this);
}

void HWNDView::Detach() {
  DCHECK(native_view());
  FocusManager::UninstallFocusSubclass(native_view());
  set_native_view(NULL);
  set_installed_clip(false);
}

void HWNDView::Paint(gfx::Canvas* canvas) {
  // The area behind our window is black, so during a fast resize (where our
  // content doesn't draw over the full size of our HWND, and the HWND
  // background color doesn't show up), we need to cover that blackness with
  // something so that fast resizes don't result in black flash.
  //
  // It would be nice if this used some approximation of the page's
  // current background color.
  if (installed_clip())
    canvas->FillRectInt(SkColorSetRGB(255, 255, 255), 0, 0, width(), height());
}

std::string HWNDView::GetClassName() const {
  return kViewClassName;
}

void HWNDView::ViewHierarchyChanged(bool is_add, View *parent, View *child) {
  if (!native_view())
    return;

  Widget* widget = GetWidget();
  if (is_add && widget) {
    HWND parent_hwnd = ::GetParent(native_view());
    HWND widget_hwnd = widget->GetNativeView();
    if (parent_hwnd != widget_hwnd)
      ::SetParent(native_view(), widget_hwnd);
    if (IsVisibleInRootView())
      ::ShowWindow(native_view(), SW_SHOW);
    else
      ::ShowWindow(native_view(), SW_HIDE);
    Layout();
  } else if (!is_add) {
    ::ShowWindow(native_view(), SW_HIDE);
    ::SetParent(native_view(), NULL);
  }
}

void HWNDView::Focus() {
  ::SetFocus(native_view());
}

void HWNDView::InstallClip(int x, int y, int w, int h) {
  HRGN clip_region = CreateRectRgn(x, y, x + w, y + h);
  // NOTE: SetWindowRgn owns the region (as well as the deleting the
  // current region), as such we don't delete the old region.
  SetWindowRgn(native_view(), clip_region, FALSE);
}

void HWNDView::UninstallClip() {
  SetWindowRgn(native_view(), 0, FALSE);
}

void HWNDView::ShowWidget(int x, int y, int w, int h) {
  UINT swp_flags = SWP_DEFERERASE |
                   SWP_NOACTIVATE |
                   SWP_NOCOPYBITS |
                   SWP_NOOWNERZORDER |
                   SWP_NOZORDER;
  // Only send the SHOWWINDOW flag if we're invisible, to avoid flashing.
  if (!::IsWindowVisible(native_view()))
    swp_flags = (swp_flags | SWP_SHOWWINDOW) & ~SWP_NOREDRAW;

  if (fast_resize()) {
    // In a fast resize, we move the window and clip it with SetWindowRgn.
    RECT win_rect;
    GetWindowRect(native_view(), &win_rect);
    gfx::Rect rect(win_rect);
    ::SetWindowPos(native_view(), 0, x, y, rect.width(), rect.height(),
                   swp_flags);

    HRGN clip_region = CreateRectRgn(0, 0, w, h);
    SetWindowRgn(native_view(), clip_region, FALSE);
    set_installed_clip(true);
  } else {
    ::SetWindowPos(native_view(), 0, x, y, w, h, swp_flags);
  }
}

void HWNDView::HideWidget() {
  if (!::IsWindowVisible(native_view()))
    return;  // Currently not visible, nothing to do.

  // The window is currently visible, but its clipped by another view. Hide
  // it.
  ::SetWindowPos(native_view(), 0, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER |
                 SWP_NOREDRAW | SWP_NOOWNERZORDER);
}

}  // namespace views
