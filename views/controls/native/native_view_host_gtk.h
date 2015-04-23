// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_NATIVE_HOST_VIEW_GTK_H_
#define VIEWS_CONTROLS_NATIVE_HOST_VIEW_GTK_H_

#include <gtk/gtk.h>
#include <string>

#include "base/logging.h"
#include "views/controls/native/native_view_host_wrapper.h"

namespace views {

class View;
class WidgetGtk;

class NativeViewHostGtk : public NativeViewHostWrapper {
 public:
  explicit NativeViewHostGtk(NativeViewHost* host);
  virtual ~NativeViewHostGtk();

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
  WidgetGtk* GetHostWidget() const;

  // Invoked from the 'destroy' signal.
  static void CallDestroy(GtkObject* object, NativeViewHostGtk* host);

  // Our associated NativeViewHost.
  NativeViewHost* host_;

  // Have we installed a region on the gfx::NativeView used to clip to only the
  // visible portion of the gfx::NativeView ?
  bool installed_clip_;

  // Signal handle id for 'destroy' signal.
  gulong destroy_signal_id_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_NATIVE_HOST_VIEW_GTK_H_

