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

#ifndef CHROME_TEST_AUTOMATION_WINDOW_PROXY_H__
#define CHROME_TEST_AUTOMATION_WINDOW_PROXY_H__

#include <string>

#include <windows.h>

#include "base/thread.h"
#include "chrome/test/automation/automation_handle_tracker.h"

class GURL;
class BrowserProxy;
class TabProxy;
class WindowProxy;

namespace gfx {
  class Rect;
}

// This class presents the interface to actions that can be performed on a given
// window.  Note that this object can be invalidated at any time if the
// corresponding window in the app is closed.  In that case, any subsequent
// calls will return false immediately.
class WindowProxy : public AutomationResourceProxy {
 public:
  WindowProxy(AutomationMessageSender* sender,
              AutomationHandleTracker* tracker,
              int handle)
    : AutomationResourceProxy(tracker, sender, handle) {}
  virtual ~WindowProxy() {}

  // Gets the outermost HWND that corresponds to the given window.
  // Returns true if the call was successful.
  bool GetHWND(HWND* handle) const;

  // Simulates a click at the OS level. |click| is in the window's coordinates
  // and |flags| specifies which buttons are pressed (as defined in
  // chrome/views/event.h).  Note that this is equivalent to the user moving
  // the mouse and pressing the button.  So if there is a window on top of this
  // window, the top window is clicked.
  bool SimulateOSClick(const POINT& click, int flags);

  // Simulates a key press at the OS level. |key| is the key pressed  and
  // |flags| specifies which modifiers keys are also pressed (as defined in
  // chrome/views/event.h).  Note that this actually sends the event to the
  // window that has focus.
  bool SimulateOSKeyPress(wchar_t key, int flags);

  // Shows/hides the window and as a result makes it active/inactive.
  // Returns true if the call was successful.
  bool SetVisible(bool visible);

  // Sets |active| to true if this view is currently the active window.
  // Returns true if the call was successful.
  bool IsActive(bool* active);

  // Make this window the active window.
  // Returns true if the call was successful.
  bool Activate();

  // Gets the bounds (in window coordinates) that correspond to the view with
  // the given ID in this window.  Returns true if bounds could be obtained.
  // If |screen_coordinates| is true, the bounds are returned in the coordinates
  // of the screen, if false in the coordinates of the browser.
  bool GetViewBounds(int view_id, gfx::Rect* bounds, bool screen_coordinates);

  // Like GetViewBounds except returns false if timeout occurs before view
  // bounds are obtained, and sets is_timeout accordingly.
  bool GetViewBoundsWithTimeout(int view_id, gfx::Rect* bounds,
                                bool screen_coordinates, uint32 timeout_ms,
                                bool* is_timeout);
  // Gets the id of the view that currently has focus.  Returns true if the id
  // was retrieved.
  bool GetFocusedViewID(int* view_id);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WindowProxy);
};

#endif // CHROME_TEST_AUTOMATION_WINDOW_PROXY_H__
