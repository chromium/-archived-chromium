// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_HWND_VIEW_H_
#define VIEWS_CONTROLS_HWND_VIEW_H_

#include <string>

#include "views/controls/native_view_host.h"

namespace views {

/////////////////////////////////////////////////////////////////////////////
//
// HWNDView class
//
//   The HWNDView class hosts a native window handle (HWND) sizing it
//   according to the bounds of the view. This is useful whenever you need to
//   show a UI control that has a HWND (e.g. a native windows Edit control)
//   within thew View hierarchy and benefit from the sizing/layout.
//
/////////////////////////////////////////////////////////////////////////////
// TODO: Rename this to NativeViewHostWin.
class HWNDView : public NativeViewHost {
 public:
  HWNDView();
  virtual ~HWNDView();

  // Attach a window handle to this View, making the window it represents
  // subject to sizing according to this View's parent container's Layout
  // Manager's sizing heuristics.
  //
  // This object should be added to the view hierarchy before calling this
  // function, which will expect the parent to be valid.
  void Attach(HWND hwnd);

  // Detach the attached window handle. It will no longer be updated
  void Detach();

  // TODO(sky): convert this to native_view().
  HWND GetHWND() const { return native_view(); }

  virtual void Paint(gfx::Canvas* canvas);

  // Overridden from View.
  virtual std::string GetClassName() const;

 protected:
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

  virtual void Focus();

  // NativeHostView overrides.
  virtual void InstallClip(int x, int y, int w, int h);
  virtual void UninstallClip();
  virtual void ShowWidget(int x, int y, int w, int h);
  virtual void HideWidget();

 private:
  DISALLOW_COPY_AND_ASSIGN(HWNDView);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_HWND_VIEW_H_
