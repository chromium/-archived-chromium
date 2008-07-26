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

#ifndef CHROME_BROWSER_CONSTRAINED_WINDOW_H__
#define CHROME_BROWSER_CONSTRAINED_WINDOW_H__

#include "chrome/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class ConstrainedWindow;
namespace ChromeViews {
class View;
class WindowDelegate;
}
namespace gfx {
class Point;
class Rect;
}
class GURL;
class TabContents;

///////////////////////////////////////////////////////////////////////////////
// ConstrainedTabContentsDelegate
//
//  An object that implements this interface is managing one or more
//  constrained windows. This interface is used to inform the delegate about
//  events within the Constrained Window.
//
class ConstrainedTabContentsDelegate {
 public:
  // Called when the contained TabContents creates a new TabContents.  The
  // ConstrainedWindow has no way to present the new TabContents, so it just
  // lets the delegate decide what to do.
  virtual void AddNewContents(ConstrainedWindow* window,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) = 0;

  // Called to open a URL in a the specified manner.
  virtual void OpenURL(ConstrainedWindow* window,
                       const GURL& url,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition) = 0;

  // Called when the window is about to be closed.
  virtual void WillClose(ConstrainedWindow* window) = 0;

  // Called when the window's contents should be detached into a top-level
  // window.  The delegate is expected to have re-parented the TabContents by
  // the time this method returns.
  // |contents_bounds| is the bounds of the TabContents after the detach
  // action. These are in screen coordinates and are for the TabContents _only_
  // - the window UI should be created around it at an appropriate size.
  // |mouse_pt| is the position of the cursor in screen coordinates.
  // |frame_component| is the part of the constrained window frame that
  // corresponds to |mouse_pt| as returned by WM_NCHITTEST.
  virtual void DetachContents(ConstrainedWindow* window,
                              TabContents* contents,
                              const gfx::Rect& contents_bounds,
                              const gfx::Point& mouse_pt,
                              int frame_component) = 0;

  // Called when the window is moved or resized.
  virtual void DidMoveOrResize(ConstrainedWindow* window) = 0;
};

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindow
//
//  This interface represents a window that is constrained to a TabContents'
//  bounds.
//
class ConstrainedWindow {
 public:
  // Create a Constrained Window that contains a ChromeViews::View subclass
  // that provides the client area. Typical uses include the HTTP Basic Auth
  // prompt. The caller must provide an object implementing
  // ChromeViews::WindowDelegate so that the Constrained Window can be properly
  // configured. If |initial_bounds| is empty, the dialog will be centered
  // within the constraining TabContents.
  static ConstrainedWindow* CreateConstrainedDialog(
      TabContents* owner,
      const gfx::Rect& initial_bounds,
      ChromeViews::View* contents_view,
      ChromeViews::WindowDelegate* window_delegate);

  // Create a Constrained Window that contains a TabContents subclass, e.g. for
  // a web popup. |initial_bounds| specifies the desired position of the
  // |constrained_contents|, not the bounds of the window itself.
  // |user_gesture| specifies whether or not this window was opened as a result
  // of a user input event, or is an unsolicited popup.
  static ConstrainedWindow* CreateConstrainedPopup(
      TabContents* owner,
      const gfx::Rect& initial_bounds,
      TabContents* constrained_contents);

  // Generates the bounds for a window when one/both of the
  // initial_bounds are invalid.
  static void GenerateInitialBounds(const gfx::Rect& initial_bounds,
                                    TabContents* parent,
                                    gfx::Rect* window_bounds);

  // Activates the Constrained Window, which has the effect of detaching it if
  // it contains a WebContents, otherwise just brings it to the front of the
  // z-order.
  virtual void ActivateConstrainedWindow() = 0;

  // Closes the Constrained Window.
  virtual void CloseConstrainedWindow() = 0;

  // Tells the Constrained Window to resize to the specified size.
  virtual void ResizeConstrainedWindow(int width, int height) = 0;

  // Repositions the Constrained Window so that the lower right corner
  // of the titlebar is at the passed in |anchor_point|.
  virtual void RepositionConstrainedWindowTo(
      const gfx::Point& anchor_point) = 0;

  // Returns true if the Constrained Window is being "suppressed" (i.e.
  // positioned to the bottom right of the constraining TabContents) because it
  // was opened without a user gesture.
  virtual bool IsSuppressedConstrainedWindow() const = 0;

  // Tells the Constrained Window that the constraining TabContents was hidden,
  // e.g. via a tab switch.
  virtual void WasHidden() = 0;

  // Tells the Constrained Window that the constraining TabContents became
  // visible, e.g. via a tab switch.
  virtual void DidBecomeSelected() = 0;

  // Returns the title of the Constrained Window.
  virtual std::wstring GetWindowTitle() const = 0;

  // Updates the Window's title and repaints the titlebar
  virtual void UpdateWindowTitle() = 0;

  // Returns the current display rectangle (relative to its
  // parent). This method is only called from the unit tests to check
  // the location/size of a constrained window.
  virtual const gfx::Rect& GetCurrentBounds() const = 0;
};

#endif  // #ifndef CHROME_BROWSER_CONSTRAINED_WINDOW_H__
