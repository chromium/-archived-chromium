// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_CONTAINER_H_
#define CHROME_VIEWS_CONTAINER_H_

// TODO(maruel):  Remove once HWND is abstracted.
#include <windows.h>

namespace gfx {
class Rect;
}

// TODO(maruel):  Remove once gfx::Rect is used instead.
namespace WTL {
class CRect;
}
using WTL::CRect;

namespace views {

class RootView;
class TooltipManager;
class Accelerator;

/////////////////////////////////////////////////////////////////////////////
//
// Container interface
//
//   Container is an abstract class that defines the API that should be
//   implemented by a CWindow / HWND implementation in order to host a view
//   hierarchy.
//
//   Container wraps a hierarchy of View objects (see view.h) that implement
//   painting and flexible layout within the bounds of the Container's window.
//
//   The Container is responsible for handling various system events and
//   forwarding them to the appropriate view.
//
/////////////////////////////////////////////////////////////////////////////

class Container {
 public:
  virtual ~Container() { }

  // Returns the bounds of this container in the screen coordinate system.
  // If the receiving view container is a frame which is larger than its
  // client area, this method returns the client area if including_frame is
  // false and the frame bounds otherwise. If the receiving view container
  // is not a frame, including_frame is ignored.
  virtual void GetBounds(CRect *out, bool including_frame) const = 0;

  // Moves this view container to the front of the Z-Order
  // If should_activate is TRUE, the window should also become the active
  // window
  virtual void MoveToFront(bool should_activate) = 0;

  // Returns the Window HWND associated with this container
  virtual HWND GetHWND() const = 0;

  // Forces a paint of a specified rectangle immediately.
  virtual void PaintNow(const gfx::Rect& update_rect) = 0;

  // Returns the RootView contained by this container
  virtual RootView* GetRootView() = 0;

  // Returns whether the view container is visible to the user
  virtual bool IsVisible() = 0;

  // Returns whether the view container is the currently active window.
  virtual bool IsActive() = 0;

  // Returns the TooltipManager for this Container. If this Container does not
  // support tooltips, NULL is returned.
  virtual TooltipManager* GetTooltipManager() {
    return NULL;
  }

  // Returns the accelerator given a command id. Returns false if there is
  // no accelerator associated with a given id, which is a common condition.
  virtual bool GetAccelerator(int cmd_id,
                              Accelerator* accelerator) = 0;
};

}  // namespace views

#endif // CHROME_VIEWS_VIEW_CONTAINER_H_

