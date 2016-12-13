// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_
#define VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_

#include "views/window/non_client_view.h"

namespace views {

class WindowWin;

class NativeFrameView : public NonClientFrameView {
 public:
  explicit NativeFrameView(WindowWin* frame);
  virtual ~NativeFrameView();

  // NonClientFrameView overrides:
  virtual gfx::Rect GetBoundsForClientView() const;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const;
  virtual gfx::Point GetSystemMenuPoint() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask);
  virtual void EnableClose(bool enable);
  virtual void ResetWindowControls();

 private:
  // Our containing frame.
  WindowWin* frame_;

  DISALLOW_COPY_AND_ASSIGN(NativeFrameView);
};

}  // namespace views

#endif  // #ifndef VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_
