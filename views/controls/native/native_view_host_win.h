// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_HWND_VIEW_H_
#define VIEWS_CONTROLS_HWND_VIEW_H_

#include "base/logging.h"
#include "views/controls/native/native_view_host_wrapper.h"

namespace views {

class NativeViewHost;

// A Windows implementation of NativeViewHostWrapper
class NativeViewHostWin : public NativeViewHostWrapper {
 public:
  explicit NativeViewHostWin(NativeViewHost* host);
  virtual ~NativeViewHostWin();

  // Overridden from NativeViewHostWrapper:
  virtual void NativeViewAttached();
  virtual void NativeViewDetaching();
  virtual void AddedToWidget();
  virtual void RemovedFromWidget();
  virtual void InstallClip(int x, int y, int w, int h);
  virtual bool HasInstalledClip();
  virtual void UninstallClip();
  virtual void ShowWidget(int x, int y, int w, int h);
  virtual void HideWidget();
  virtual void SetFocus();

 private:
  // Our associated NativeViewHost.
  NativeViewHost* host_;

  // Have we installed a region on the gfx::NativeView used to clip to only the
  // visible portion of the gfx::NativeView ?
  bool installed_clip_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostWin);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_HWND_VIEW_H_
