// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  // Returns the browser this window corresponds to, or NULL if this window
  // is not a browser.  The caller owns the returned BrowserProxy.
  BrowserProxy* GetBrowser();

  // Same as GetWindow except return NULL if response isn't received
  // before the specified timeout.
  BrowserProxy* GetBrowserWithTimeout(uint32 timeout_ms, bool* is_timeout);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WindowProxy);
};

#endif // CHROME_TEST_AUTOMATION_WINDOW_PROXY_H__

