// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_WRAPPER_H_
#define VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_WRAPPER_H_

namespace views {

class NativeViewHost;

// An interface that implemented by an object that wraps a gfx::NativeView on
// a specific platform, used to perform platform specific operations on that
// native view when attached, detached, moved and sized.
class NativeViewHostWrapper {
 public:
  virtual ~NativeViewHostWrapper() {}

  // Called when a gfx::NativeView has been attached to the associated
  // NativeViewHost, allowing the wrapper to perform platform-specific
  // initialization.
  virtual void NativeViewAttached() = 0;

  // Called before the attached gfx::NativeView is detached from the
  // NativeViewHost, allowing the wrapper to perform platform-specific
  // cleanup.
  virtual void NativeViewDetaching() = 0;

  // Called when our associated NativeViewHost is added to a View hierarchy
  // rooted at a valid Widget.
  virtual void AddedToWidget() = 0;

  // Called when our associated NativeViewHost is removed from a View hierarchy
  // rooted at a valid Widget.
  virtual void RemovedFromWidget() = 0;

  // Installs a clip on the gfx::NativeView.
  virtual void InstallClip(int x, int y, int w, int h) = 0;

  // Whether or not a clip has been installed on the wrapped gfx::NativeView.
  virtual bool HasInstalledClip() = 0;

  // Removes the clip installed on the gfx::NativeView by way of InstallClip.
  virtual void UninstallClip() = 0;

  // Shows the gfx::NativeView at the specified position (relative to the parent
  // native view).
  virtual void ShowWidget(int x, int y, int w, int h) = 0;

  // Hides the gfx::NativeView. NOTE: this may be invoked when the native view
  // is already hidden.
  virtual void HideWidget() = 0;

  // Sets focus to the gfx::NativeView.
  virtual void SetFocus() = 0;

  // Creates a platform-specific instance of an object implementing this
  // interface.
  static NativeViewHostWrapper* CreateWrapper(NativeViewHost* host);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_WRAPPER_H_
