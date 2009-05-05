// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/controls/native_view_host.h"

#include "chrome/views/widget/widget.h"
#include "base/logging.h"

namespace views {

NativeViewHost::NativeViewHost()
    : native_view_(NULL),
      installed_clip_(false),
      fast_resize_(false),
      focus_view_(NULL) {
  // The native widget is placed relative to the root. As such, we need to
  // know when the position of any ancestor changes, or our visibility relative
  // to other views changed as it'll effect our position relative to the root.
  SetNotifyWhenVisibleBoundsInRootChanges(true);
}

NativeViewHost::~NativeViewHost() {
}

gfx::Size NativeViewHost::GetPreferredSize() {
  return preferred_size_;
}

void NativeViewHost::Layout() {
  if (!native_view_)
    return;

  // Since widgets know nothing about the View hierarchy (they are direct
  // children of the Widget that hosts our View hierarchy) they need to be
  // positioned in the coordinate system of the Widget, not the current
  // view.
  gfx::Point top_left;
  ConvertPointToWidget(this, &top_left);

  gfx::Rect vis_bounds = GetVisibleBounds();
  bool visible = !vis_bounds.IsEmpty();

  if (visible && !fast_resize_) {
    if (vis_bounds.size() != size()) {
      // Only a portion of the Widget is really visible.
      int x = vis_bounds.x();
      int y = vis_bounds.y();
      InstallClip(x, y, vis_bounds.width(), vis_bounds.height());
      installed_clip_ = true;
    } else if (installed_clip_) {
      // The whole widget is visible but we installed a clip on the widget,
      // uninstall it.
      UninstallClip();
      installed_clip_ = false;
    }
  }

  if (visible) {
    ShowWidget(top_left.x(), top_left.y(), width(), height());
  } else {
    HideWidget();
  }
}

void NativeViewHost::VisibilityChanged(View* starting_from, bool is_visible) {
  Layout();
}

void NativeViewHost::VisibleBoundsInRootChanged() {
  Layout();
}

}  // namespace views
