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

#ifndef CHROME_BROWSER_TAB_CONTENTS_DELEGATE_H_
#define CHROME_BROWSER_TAB_CONTENTS_DELEGATE_H_

// TODO(maruel):  Remove once UINT and HWND are replaced / typedef.
#include <windows.h>

#include "chrome/browser/page_navigator.h"
#include "chrome/common/navigation_types.h"

namespace gfx {
class Point;
class Rect;
}

class TabContents;
class HtmlDialogContentsDelegate;

// Objects implement this interface to get notified about changes in the
// TabContents and to provide necessary functionality.
class TabContentsDelegate : public PageNavigator {
 public:
  // Opens a new URL inside the passed in TabContents, if source is 0 open
  // in the current front-most tab.
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition) = 0;

  virtual void OpenURL(const GURL& url,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition)
  {
    OpenURLFromTab(NULL, url, disposition, transition);
  }

  // Called to inform the delegate that the tab content's navigation state
  // changed. The |changed_flags| indicates the parts of the navigation state
  // that have been updated, and is any combination of the
  // |TabContents::InvalidateTypes| bits.
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) = 0;

  // Called to cause the delegate to replace the source contents with the new
  // contents.
  virtual void ReplaceContents(TabContents* source,
                               TabContents* new_contents) = 0;

  // Creates a new tab with the already-created TabContents 'new_contents'.
  // The window for the added contents should be reparented correctly when this
  // method returns.  If |disposition| is NEW_POPUP, |pos| should hold the
  // initial position.
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) = 0;

  // Called when while dragging constrained TabContents, the mouse pointer
  // moves outside the bounds of the constraining contents. The delegate can
  // use this as an opportunity to continue the drag in a detached window.
  // |contents_bounds| is the bounds of the constrained TabContents in screen
  // coordinates.
  // |mouse_pt| is the position of the mouse pointer in screen coordinates.
  // |frame_component| is the part of the constrained window frame that
  // corresponds to |mouse_pt| as returned by WM_NCHITTEST.
  virtual void StartDraggingDetachedContents(
      TabContents* source,
      TabContents* new_contents,
      const gfx::Rect& contents_bounds,
      const gfx::Point& mouse_pt,
      int frame_component) { }

  // Selects the specified contents, bringing its container to the front.
  virtual void ActivateContents(TabContents* contents) = 0;

  // Notifies the delegate that this contents is starting or is done loading
  // some resource. The delegate should use this notification to represent
  // loading feedback. See TabContents::is_loading()
  virtual void LoadingStateChanged(TabContents* source) = 0;

  // Request the delegate to close this tab contents, and do whatever cleanup
  // it needs to do.
  virtual void CloseContents(TabContents* source) = 0;

  // Request the delegate to move this tab contents to the specified position
  // in screen coordinates.
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) = 0;

  // Called to determine if the TabContents is contained in a popup window.
  virtual bool IsPopup(TabContents* source) = 0;

  // Returns the tab which contains the specified tab content if it is
  // constrained, NULL otherwise.
  virtual TabContents* GetConstrainingContents(TabContents* source) {
    return NULL;
  }

  // Notification that some of our content has changed size as
  // part of an animation.
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) = 0;

  // Notification that the starredness of the current URL changed.
  virtual void URLStarredChanged(TabContents* source, bool starred) = 0;

  // Notification that the target URL has changed
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) = 0;

  // Notification that the target URL has changed
  virtual void ContentsMouseEvent(TabContents* source, UINT message) { }

  // Request the delegate to change the zoom level of the current tab.
  virtual void ContentsZoomChange(bool zoom_in) { }

  // Check whether this contents is inside a window dedicated to running a web
  // application.
  virtual bool IsApplication() { return false; }

  // Detach the given tab and convert it to a "webapp" view.  The tab must be
  // a WebContents with a valid WebApp set.
  virtual void ConvertContentsToApplication(TabContents* source) { }

  // Notifies the delegate that a navigation happened. nav_type indicates the
  // type of navigation. If nav_type is NAVIGATION_BACK_FORWARD then the
  // relative_navigation_offset indicates the relative offset of the navigation
  // within the session history (a negative value indicates a backward
  // navigation and a positive value indicates a forward navigation). If
  // nav_type is any other value, the relative_navigation_offset parameter
  // is not defined and should be ignored.
  virtual void DidNavigate(NavigationType nav_type,
                           int relative_navigation_offset) { return; }

  // Informs the TabContentsDelegate that some of our state has changed 
  // for this tab.
  virtual void ContentsStateChanged(TabContents* source) {}

  // Return whether this tab contents should have a URL bar. Only web contents
  // opened with a minimal chrome and their popups can be displayed without a
  // URL bar.
  virtual bool ShouldDisplayURLField() { return true; }

  // Whether this tab can be blurred through a javascript obj.blur()
  // call. ConstrainedWindows shouldn't be able to be blurred.
  virtual bool CanBlur() const { return true; }

  // Show a dialog with HTML content. |delegate| contains a pointer to the
  // delegate who knows how to display the dialog (which file URL and JSON
  // string input to use during initialization). |parent_hwnd| is the window
  // that should be parent of the dialog, or NULL for the default.
  virtual void ShowHtmlDialog(HtmlDialogContentsDelegate* delegate,
                              HWND parent_hwnd) { }

  // Tells us that we've finished firing this tab's beforeunload event.
  // The proceed bool tells us whether the user chose to proceed closing the
  // tab. Returns true if the tab can continue on firing it's unload event.
  // If we're closing the entire browser, then we'll want to delay firing 
  // unload events until all the beforeunload events have fired.
  virtual void BeforeUnloadFired(TabContents* tab,
                                 bool proceed, 
                                 bool* proceed_to_fire_unload) { 
    *proceed_to_fire_unload = true;
  }
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_DELEGATE_H_
