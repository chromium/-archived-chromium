// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_WIDGET_H_
#define CHROME_VIEWS_WIDGET_H_

#include "base/gfx/native_widget_types.h"

namespace gfx {
class Rect;
}

namespace views {

class Accelerator;
class RootView;
class TooltipManager;
class Window;

////////////////////////////////////////////////////////////////////////////////
//
// Widget interface
//
//   Widget is an abstract class that defines the API that should be implemented
//   by a native window in order to host a view hierarchy.
//
//   Widget wraps a hierarchy of View objects (see view.h) that implement
//   painting and flexible layout within the bounds of the Widget's window.
//
//   The Widget is responsible for handling various system events and forwarding
//   them to the appropriate view.
//
/////////////////////////////////////////////////////////////////////////////

class Widget {
 public:
  virtual ~Widget() { }

  // Returns the bounds of this Widget in the screen coordinate system.
  // If the receiving Widget is a frame which is larger than its client area,
  // this method returns the client area if including_frame is false and the
  // frame bounds otherwise. If the receiving Widget is not a frame,
  // including_frame is ignored.
  virtual void GetBounds(gfx::Rect* out, bool including_frame) const = 0;

  // Moves this Widget to the front of the Z-Order If should_activate is TRUE,
  // the window should also become the active window.
  virtual void MoveToFront(bool should_activate) = 0;

  // Returns the gfx::NativeView associated with this Widget.
  virtual gfx::NativeView GetNativeView() const = 0;

  // Forces a paint of a specified rectangle immediately.
  virtual void PaintNow(const gfx::Rect& update_rect) = 0;

  // Returns the RootView contained by this Widget.
  virtual RootView* GetRootView() = 0;

  // Returns whether the Widget is visible to the user.
  virtual bool IsVisible() = 0;

  // Returns whether the Widget is the currently active window.
  virtual bool IsActive() = 0;

  // Returns the TooltipManager for this Widget. If this Widget does not support
  // tooltips, NULL is returned.
  virtual TooltipManager* GetTooltipManager() {
    return NULL;
  }

  // Returns the accelerator given a command id. Returns false if there is
  // no accelerator associated with a given id, which is a common condition.
  virtual bool GetAccelerator(int cmd_id,
                              Accelerator* accelerator) = 0;

  // Returns the Widget as a Window, if such a conversion is possible, or NULL
  // if it is not.
  virtual Window* AsWindow() { return NULL; }
  virtual const Window* AsWindow() const { return NULL; }
};

}  // namespace views

#endif // CHROME_VIEWS_WIDGET_H_
