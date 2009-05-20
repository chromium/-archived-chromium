// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_NATIVE_HOST_VIEW_GTK_H_
#define VIEWS_CONTROLS_NATIVE_HOST_VIEW_GTK_H_

#include <gtk/gtk.h>
#include <string>

#include "views/controls/native_view_host.h"

namespace views {

class NativeViewHostGtk : public NativeViewHost {
 public:
  NativeViewHostGtk();
  virtual ~NativeViewHostGtk();

  // Sets and retrieves the View associated with a particular widget.
  static View* GetViewForNative(GtkWidget* widget);
  static void SetViewForNative(GtkWidget* widget, View* view);

  // Attach a widget to this View, making the window it represents
  // subject to sizing according to this View's parent container's Layout
  // Manager's sizing heuristics.
  //
  // This object should be added to the view hierarchy before calling this
  // function, which will expect the parent to be valid.

  // TODO: figure out ownership!
  void Attach(gfx::NativeView w);

  // Detach the attached widget handle. It will no longer be updated
  void Detach();

 protected:
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

  virtual void Focus();

  // NativeHostView overrides.
  virtual void InstallClip(int x, int y, int w, int h);
  virtual void UninstallClip();
  virtual void ShowWidget(int x, int y, int w, int h);
  virtual void HideWidget();

 private:
  // Signal handle id for 'destroy' signal.
  gulong destroy_signal_id_;

  // Invoked from the 'destroy' signal.
  void OnDestroy();

  static void CallDestroy(GtkObject* object);

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_NATIVE_HOST_VIEW_GTK_H_
