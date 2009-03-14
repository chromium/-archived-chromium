// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_NATIVE_FRAME_VIEW_H_
#define CHROME_VIEWS_NATIVE_FRAME_VIEW_H_

#include "chrome/views/non_client_view.h"

namespace views {

class NativeFrameView : public NonClientFrameView {
 public:
  explicit NativeFrameView(Window* frame);
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
  Window* frame_;

  DISALLOW_COPY_AND_ASSIGN(NativeFrameView);
};

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_NATIVE_FRAME_VIEW_H_
