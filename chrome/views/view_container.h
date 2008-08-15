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

#ifndef CHROME_VIEWS_VIEW_CONTAINER_H_
#define CHROME_VIEWS_VIEW_CONTAINER_H_

// TODO(maruel):  Remove once HWND is abstracted.
#include <windows.h>

// TODO(maruel):  Remove once gfx::Rect is used instead.
namespace WTL {
class CRect;
}
using WTL::CRect;

namespace ChromeViews {

class RootView;
class TooltipManager;
class Accelerator;

/////////////////////////////////////////////////////////////////////////////
//
// ViewContainer class
//
//   ViewContainer is an abstract class that defines the API that should
//   be implemented by a CWindow / HWND implementation in order to host a view
//   hierarchy.
//
//   ViewContainer wraps a hierarchy of View objects (see view.h) that
//   implement painting and flexible layout within the bounds of the
//   ViewContainer's window.
//
//   The ViewContainer is responsible for handling various system events and
//   forwarding them to the appropriate view.
//
/////////////////////////////////////////////////////////////////////////////

class ViewContainer {
 public:
  virtual ~ViewContainer() { }

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
  virtual void PaintNow(const CRect& update_rect) = 0;

  // Returns the RootView contained by this container
  virtual RootView* GetRootView() = 0;

  // Returns whether the view container is visible to the user
  virtual bool IsVisible() = 0;

  // Returns whether the view container is the currently active window.
  virtual bool IsActive() = 0;

  // Returns the TooltipManager for this ViewContainer. If this ViewContainer
  // does not support tooltips, NULL is returned.
  virtual TooltipManager* GetTooltipManager() {
    return NULL;
  }

  // Returns the accelerator given a command id. Returns false if there is
  // no accelerator associated with a given id, which is a common condition.
  virtual bool GetAccelerator(int cmd_id,
                              Accelerator* accelerator) = 0;
};

}  // namespace ChromeViews

#endif // CHROME_VIEWS_VIEW_CONTAINER_H_
