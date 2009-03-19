// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/controls/hwnd_view.h"

#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/win_util.h"
#include "chrome/views/focus/focus_manager.h"
#include "chrome/views/controls/scroll_view.h"
#include "chrome/views/widget/widget.h"
#include "base/logging.h"

namespace views {

static const char kViewClassName[] = "chrome/views/HWNDView";

HWNDView::HWNDView() :
    hwnd_(0),
    installed_clip_(false),
    fast_resize_(false),
    focus_view_(NULL) {
  // HWNDs are placed relative to the root. As such, we need to
  // know when the position of any ancestor changes, or our visibility relative
  // to other views changed as it'll effect our position relative to the root.
  SetNotifyWhenVisibleBoundsInRootChanges(true);
}

HWNDView::~HWNDView() {
}

void HWNDView::Attach(HWND hwnd) {
  DCHECK(hwnd_ == NULL);
  DCHECK(hwnd) << "Impossible detatched tab case; See crbug.com/6316";

  hwnd_ = hwnd;

  // First hide the new window. We don't want anything to draw (like sub-hwnd
  // borders), when we change the parent below.
  ShowWindow(hwnd_, SW_HIDE);

  // Need to set the HWND's parent before changing its size to avoid flashing.
  ::SetParent(hwnd_, GetWidget()->GetNativeView());
  Layout();

  // Register with the focus manager so the associated view is focused when the
  // native control gets the focus.
  FocusManager::InstallFocusSubclass(hwnd_, focus_view_ ? focus_view_ : this);
}

void HWNDView::Detach() {
  DCHECK(hwnd_);
  FocusManager::UninstallFocusSubclass(hwnd_);
  hwnd_ = NULL;
  installed_clip_ = false;
}

void HWNDView::SetAssociatedFocusView(View* view) {
  DCHECK(!::IsWindow(hwnd_));
  focus_view_ = view;
}

HWND HWNDView::GetHWND() const {
  return hwnd_;
}

void HWNDView::Layout() {
  if (!hwnd_)
    return;

  // Since HWNDs know nothing about the View hierarchy (they are direct
  // children of the Widget that hosts our View hierarchy) they need to be
  // positioned in the coordinate system of the Widget, not the current
  // view.
  gfx::Point top_left;
  ConvertPointToWidget(this, &top_left);

  gfx::Rect vis_bounds = GetVisibleBounds();
  bool visible = !vis_bounds.IsEmpty();

  if (visible && !fast_resize_) {
    if (vis_bounds.size() != size()) {
      // Only a portion of the HWND is really visible.
      int x = vis_bounds.x();
      int y = vis_bounds.y();
      HRGN clip_region = CreateRectRgn(x, y, x + vis_bounds.width(),
                                       y + vis_bounds.height());
      // NOTE: SetWindowRgn owns the region (as well as the deleting the
      // current region), as such we don't delete the old region.
      SetWindowRgn(hwnd_, clip_region, FALSE);
      installed_clip_ = true;
    } else if (installed_clip_) {
      // The whole HWND is visible but we installed a clip on the HWND,
      // uninstall it.
      SetWindowRgn(hwnd_, 0, FALSE);
      installed_clip_ = false;
    }
  }

  if (visible) {
    UINT swp_flags;
    swp_flags = SWP_DEFERERASE |
                SWP_NOACTIVATE |
                SWP_NOCOPYBITS |
                SWP_NOOWNERZORDER |
                SWP_NOZORDER;
    // Only send the SHOWWINDOW flag if we're invisible, to avoid flashing.
    if (!::IsWindowVisible(hwnd_))
      swp_flags = (swp_flags | SWP_SHOWWINDOW) & ~SWP_NOREDRAW;

    if (fast_resize_) {
      // In a fast resize, we move the window and clip it with SetWindowRgn.
      CRect rect;
      GetWindowRect(hwnd_, &rect);
      ::SetWindowPos(hwnd_, 0, top_left.x(), top_left.y(), rect.Width(),
                     rect.Height(), swp_flags);

      HRGN clip_region = CreateRectRgn(0, 0, width(), height());
      SetWindowRgn(hwnd_, clip_region, FALSE);
      installed_clip_ = true;
    } else {
      ::SetWindowPos(hwnd_, 0, top_left.x(), top_left.y(), width(), height(),
                     swp_flags);
    }
  } else if (::IsWindowVisible(hwnd_)) {
    // The window is currently visible, but its clipped by another view. Hide
    // it.
    ::SetWindowPos(hwnd_, 0, 0, 0, 0, 0,
                   SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER |
                   SWP_NOREDRAW | SWP_NOOWNERZORDER);
  }
}

void HWNDView::VisibilityChanged(View* starting_from, bool is_visible) {
  Layout();
}

gfx::Size HWNDView::GetPreferredSize() {
  return preferred_size_;
}

void HWNDView::ViewHierarchyChanged(bool is_add, View *parent, View *child) {
  if (hwnd_) {
    Widget* widget = GetWidget();
    if (is_add && widget) {
      HWND parent_hwnd = ::GetParent(hwnd_);
      HWND widget_hwnd = widget->GetNativeView();
      if (parent_hwnd != widget_hwnd) {
        ::SetParent(hwnd_, widget_hwnd);
      }
      if (IsVisibleInRootView())
        ::ShowWindow(hwnd_, SW_SHOW);
      else
        ::ShowWindow(hwnd_, SW_HIDE);
      Layout();
    } else if (!is_add) {
      ::ShowWindow(hwnd_, SW_HIDE);
      ::SetParent(hwnd_, NULL);
    }
  }
}

void HWNDView::VisibleBoundsInRootChanged() {
  Layout();
}

void HWNDView::Focus() {
  ::SetFocus(hwnd_);
}

void HWNDView::Paint(ChromeCanvas* canvas) {
  // The area behind our window is black, so during a fast resize (where our
  // content doesn't draw over the full size of our HWND, and the HWND
  // background color doesn't show up), we need to cover that blackness with
  // something so that fast resizes don't result in black flash.
  //
  // It would be nice if this used some approximation of the page's
  // current background color.
  if (installed_clip_)
    canvas->FillRectInt(SkColorSetRGB(255, 255, 255), 0, 0, width(), height());
}

std::string HWNDView::GetClassName() const {
  return kViewClassName;
}

}  // namespace views
