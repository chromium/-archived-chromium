// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_NATIVE_VIEW_HOST_H_
#define CHROME_VIEWS_CONTROLS_NATIVE_VIEW_HOST_H_

#include <string>

#include "chrome/views/view.h"

#include "base/gfx/native_widget_types.h"

namespace views {

// Base class for embedding native widgets in a view.
class NativeViewHost : public View {
 public:
  NativeViewHost();
  virtual ~NativeViewHost();

  void set_preferred_size(const gfx::Size& size) { preferred_size_ = size; }

  // Returns the preferred size set via set_preferred_size.
  virtual gfx::Size GetPreferredSize();

  // Overriden to invoke Layout.
  virtual void VisibilityChanged(View* starting_from, bool is_visible);

  // Invokes any of InstallClip, UninstallClip, ShowWidget or HideWidget
  // depending upon what portion of the widget is view in the parent.
  virtual void Layout();

  // A NativeViewHost has an associated focus View so that the focus of the
  // native control and of the View are kept in sync. In simple cases where the
  // NativeViewHost directly wraps a native window as is, the associated view
  // is this View. In other cases where the NativeViewHost is part of another
  // view (such as TextField), the actual View is not the NativeViewHost and
  // this method must be called to set that.
  // This method must be called before Attach().
  void SetAssociatedFocusView(View* view) { focus_view_ = view; }
  View* associated_focus_view() { return focus_view_; }

  void set_fast_resize(bool fast_resize) { fast_resize_ = fast_resize; }
  bool fast_resize() const { return fast_resize_; }

  // The embedded native view.
  gfx::NativeView native_view() const { return native_view_; }

 protected:
  // Notification that our visible bounds relative to the root has changed.
  // Invokes Layout to make sure the widget is positioned correctly.
  virtual void VisibleBoundsInRootChanged();

  // Sets the native view. Subclasses will typically invoke Layout after setting
  // the widget.
  void set_native_view(gfx::NativeView widget) { native_view_ = widget; }

  // Installs a clip on the native widget.
  virtual void InstallClip(int x, int y, int w, int h) = 0;

  // Removes the clip installed on the native widget by way of InstallClip.
  virtual void UninstallClip() = 0;

  // Shows the widget at the specified position (relative to the parent widget).
  virtual void ShowWidget(int x, int y, int w, int h) = 0;

  // Hides the widget. NOTE: this may be invoked when the widget is already
  // hidden.
  virtual void HideWidget() = 0;

  void set_installed_clip(bool installed_clip) {
    installed_clip_ = installed_clip;
  }
  bool installed_clip() const { return installed_clip_; }

 private:
  gfx::NativeView native_view_;

  // The preferred size of this View
  gfx::Size preferred_size_;

  // Have we installed a region on the HWND used to clip to only the visible
  // portion of the HWND?
  bool installed_clip_;

  // Fast resizing will move the hwnd and clip its window region, this will
  // result in white areas and will not resize the content (so scrollbars
  // will be all wrong and content will flow offscreen). Only use this
  // when you're doing extremely quick, high-framerate vertical resizes
  // and don't care about accuracy. Make sure you do a real resize at the
  // end. USE WITH CAUTION.
  bool fast_resize_;

  // The view that should be given focus when this NativeViewHost is focused.
  View* focus_view_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHost);
};

}  // namespace views

#endif  // CHROME_VIEWS_CONTROLS_NATIVE_VIEW_HOST_H_
