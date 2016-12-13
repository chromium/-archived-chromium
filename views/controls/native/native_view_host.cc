// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/native/native_view_host.h"

#include "base/logging.h"
#include "app/gfx/canvas.h"
#include "views/controls/native/native_view_host_wrapper.h"
#include "views/widget/widget.h"

namespace views {

// static
const char NativeViewHost::kViewClassName[] = "views/NativeViewHost";

////////////////////////////////////////////////////////////////////////////////
// NativeViewHost, public:

NativeViewHost::NativeViewHost()
    : native_view_(NULL),
      fast_resize_(false),
      focus_view_(NULL) {
  // The native widget is placed relative to the root. As such, we need to
  // know when the position of any ancestor changes, or our visibility relative
  // to other views changed as it'll effect our position relative to the root.
  SetNotifyWhenVisibleBoundsInRootChanges(true);
}

NativeViewHost::~NativeViewHost() {
}

void NativeViewHost::Attach(gfx::NativeView native_view) {
  DCHECK(!native_view_);
  native_view_ = native_view;
  native_wrapper_->NativeViewAttached();
}

void NativeViewHost::Detach() {
  DCHECK(native_view_);
  native_wrapper_->NativeViewDetaching();
  native_view_ = NULL;
}

void NativeViewHost::SetPreferredSize(const gfx::Size& size) {
  preferred_size_ = size;
  PreferredSizeChanged();
}

void NativeViewHost::NativeViewDestroyed() {
  // TODO(beng): figure out if this should/could call Detach instead since as it
  //             stands right now this object is left in an inconsistent state.
  native_view_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHost, View overrides:

gfx::Size NativeViewHost::GetPreferredSize() {
  return preferred_size_;
}

void NativeViewHost::Layout() {
  if (!native_view_ || !native_wrapper_.get())
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
      native_wrapper_->InstallClip(x, y, vis_bounds.width(),
                                   vis_bounds.height());
    } else if (native_wrapper_->HasInstalledClip()) {
      // The whole widget is visible but we installed a clip on the widget,
      // uninstall it.
      native_wrapper_->UninstallClip();
    }
  }

  if (visible)
    native_wrapper_->ShowWidget(top_left.x(), top_left.y(), width(), height());
  else
    native_wrapper_->HideWidget();
}

void NativeViewHost::Paint(gfx::Canvas* canvas) {
  // The area behind our window is black, so during a fast resize (where our
  // content doesn't draw over the full size of our native view, and the native
  // view background color doesn't show up), we need to cover that blackness
  // with something so that fast resizes don't result in black flash.
  //
  // It would be nice if this used some approximation of the page's
  // current background color.
  if (native_wrapper_->HasInstalledClip())
    canvas->FillRectInt(SK_ColorWHITE, 0, 0, width(), height());
}

void NativeViewHost::VisibilityChanged(View* starting_from, bool is_visible) {
  Layout();
}

void NativeViewHost::VisibleBoundsInRootChanged() {
  Layout();
}

void NativeViewHost::ViewHierarchyChanged(bool is_add, View* parent,
                                          View* child) {
  if (is_add && GetWidget()) {
    if (!native_wrapper_.get())
      native_wrapper_.reset(NativeViewHostWrapper::CreateWrapper(this));
    native_wrapper_->AddedToWidget();
  } else if (!is_add) {
    native_wrapper_->RemovedFromWidget();
  }
}

std::string NativeViewHost::GetClassName() const {
  return kViewClassName;
}

void NativeViewHost::Focus() {
  native_wrapper_->SetFocus();
}

}  // namespace views
