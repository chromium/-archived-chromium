// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_VIEWS_NON_CLIENT_VIEW_H_
#define CHROME_VIEWS_NON_CLIENT_VIEW_H_

#include "chrome/views/view.h"

namespace gfx {
class Path;
}

namespace ChromeViews {

class ClientView;

///////////////////////////////////////////////////////////////////////////////
// NonClientView
//
//  An object implementing the NonClientView interface is a View that provides
//  the "non-client" areas of a window. This is the area that typically
//  encompasses the window frame - title bar, sizing borders and window
//  controls. This interface provides methods that allow a specific
//  presentation to define non-client areas for windows hit testing, the shape
//  of the window, and other window-related information.
//
class NonClientView : public View {
 public:
  // Calculates the bounds of the client area of the window assuming the
  // window is sized to |width| and |height|.
  virtual gfx::Rect CalculateClientAreaBounds(int width,
                                              int height) const = 0;

  // Calculates the size of window required to display a client area of the
  // specified width and height.
  virtual gfx::Size CalculateWindowSizeForClientSize(int width,
                                                     int height) const = 0;

  // Returns the point, in screen coordinates, where the system menu should
  // be shown so it shows up anchored to the system menu icon.
  virtual CPoint GetSystemMenuPoint() const = 0;

  // Determines the windows HT* code when the mouse cursor is at the
  // specified point, in window coordinates.
  virtual int NonClientHitTest(const gfx::Point& point) = 0;

  // Returns a mask to be used to clip the top level window for the given
  // size. This is used to create the non-rectangular window shape.
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) = 0;

  // Toggles the enable state for the Close button (and the Close menu item in
  // the system menu).
  virtual void EnableClose(bool enable) = 0;

  // Prevents the non-client view from rendering as inactive when called with
  // |disable| true, until called with false.
  void set_paint_as_active(bool paint_as_active) { 
    paint_as_active_ = paint_as_active;
  }

 protected:
  NonClientView() : paint_as_active_(false) {}

  // Helper for non-client view implementations to determine which area of the
  // window border the specified |point| falls within. The other parameters are
  // the size of the sizing edges, and whether or not the window can be
  // resized.
  int GetHTComponentForFrame(const gfx::Point& point,
                             int resize_area_size,
                             int resize_area_corner_size,
                             int top_resize_area_size,
                             bool can_resize);

  // Accessor for paint_as_active_.
  bool paint_as_active() const { return paint_as_active_; }

 private:
  // True when the non-client view should always be rendered as if the window
  // were active, regardless of whether or not the top level window actually
  // is active.
  bool paint_as_active_;
};

}  // namespace ChromeViews

#endif  // #ifndef CHROME_VIEWS_NON_CLIENT_VIEW_H_
