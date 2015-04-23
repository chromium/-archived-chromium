// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_NATIVE_VIEW_HOST_H_
#define VIEWS_CONTROLS_NATIVE_VIEW_HOST_H_

#include <string>

#include "views/view.h"

#include "base/gfx/native_widget_types.h"

namespace views {

class NativeViewHostWrapper;

// A View type that hosts a gfx::NativeView. The bounds of the native view are
// kept in sync with the bounds of this view as it is moved and sized.
// Under the hood, a platform-specific NativeViewHostWrapper implementation does
// the platform-specific work of manipulating the underlying OS widget type.
class NativeViewHost : public View {
 public:
  // The NativeViewHost's class name.
  static const char kViewClassName[];

  NativeViewHost();
  virtual ~NativeViewHost();

  // Attach a gfx::NativeView to this View. Its bounds will be kept in sync
  // with the bounds of this View until Detach is called.
  //
  // Because native views are positioned in the coordinates of their parent
  // native view, this function should only be called after this View has been
  // added to a View hierarchy hosted within a valid Widget.
  void Attach(gfx::NativeView native_view);

  // Detach the attached window handle. Its bounds and visibility will no longer
  // be manipulated by this View.
  void Detach();

  // Sets a preferred size for the native view attached to this View.
  void SetPreferredSize(const gfx::Size& size);

  // A NativeViewHost has an associated focus View so that the focus of the
  // native control and of the View are kept in sync. In simple cases where the
  // NativeViewHost directly wraps a native window as is, the associated view
  // is this View. In other cases where the NativeViewHost is part of another
  // view (such as TextField), the actual View is not the NativeViewHost and
  // this method must be called to set that.
  // This method must be called before Attach().
  void set_focus_view(View* view) { focus_view_ = view; }
  View* focus_view() { return focus_view_; }

  // Fast resizing will move the native view and clip its visible region, this
  // will result in white areas and will not resize the content (so scrollbars
  // will be all wrong and content will flow offscreen). Only use this
  // when you're doing extremely quick, high-framerate vertical resizes
  // and don't care about accuracy. Make sure you do a real resize at the
  // end. USE WITH CAUTION.
  void set_fast_resize(bool fast_resize) { fast_resize_ = fast_resize; }
  bool fast_resize() const { return fast_resize_; }

  // Accessor for |native_view_|.
  gfx::NativeView native_view() const { return native_view_; }

  void NativeViewDestroyed();

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual void VisibilityChanged(View* starting_from, bool is_visible);

 protected:
  virtual void VisibleBoundsInRootChanged();
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual std::string GetClassName() const;
  virtual void Focus();

 private:
  // The attached native view.
  gfx::NativeView native_view_;

  // A platform-specific wrapper that does the OS-level manipulation of the
  // attached gfx::NativeView.
  scoped_ptr<NativeViewHostWrapper> native_wrapper_;

  // The preferred size of this View
  gfx::Size preferred_size_;

  // True if the native view is being resized using the fast method described
  // in the setter/accessor above.
  bool fast_resize_;

  // The view that should be given focus when this NativeViewHost is focused.
  View* focus_view_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHost);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_NATIVE_VIEW_HOST_H_
