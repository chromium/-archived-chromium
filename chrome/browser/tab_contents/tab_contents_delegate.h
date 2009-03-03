// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_DELEGATE_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_DELEGATE_H_

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/common/navigation_types.h"

class TabContents;
class HtmlDialogContentsDelegate;

// Objects implement this interface to get notified about changes in the
// TabContents and to provide necessary functionality.
class TabContentsDelegate : public PageNavigator {
 public:
  // Opens a new URL inside the passed in TabContents (if source is 0 open
  // in the current front-most tab), unless |disposition| indicates the url
  // should be opened in a new tab or window.
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition) = 0;

  virtual void OpenURL(const GURL& url, const GURL& referrer,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition) {
    OpenURLFromTab(NULL, url, referrer, disposition, transition);
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
  virtual void ContentsMouseEvent(TabContents* source, uint32 message) { }

  // Request the delegate to change the zoom level of the current tab.
  virtual void ContentsZoomChange(bool zoom_in) { }

  // Check whether this contents is inside a window dedicated to running a web
  // application.
  virtual bool IsApplication() { return false; }

  // Detach the given tab and convert it to a "webapp" view.  The tab must be
  // a WebContents with a valid WebApp set.
  virtual void ConvertContentsToApplication(TabContents* source) { }

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

  // Return the rect where to display the resize corner, if any, otherwise
  // an empty rect.
  virtual gfx::Rect GetRootWindowResizerRect() const { return gfx::Rect(); }

  // Show a dialog with HTML content. |delegate| contains a pointer to the
  // delegate who knows how to display the dialog (which file URL and JSON
  // string input to use during initialization). |parent_window| is the window
  // that should be parent of the dialog, or NULL for the default.
  virtual void ShowHtmlDialog(HtmlDialogContentsDelegate* delegate,
                              void* parent_window) { }

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

  // Send IPC to external host. Default implementation is do nothing.
  virtual void ForwardMessageToExternalHost(const std::string& message) {}

  // If the delegate is hosting tabs externally.
  virtual bool IsExternalTabContainer() const { return false; }

  // Sets focus to the location bar or some other place that is appropriate.
  // This is called when the tab wants to encourage user input, like for the
  // new tab page.
  virtual void SetFocusToLocationBar() {}

  // Called when a popup select is about to be displayed. The delegate can use
  // this to disable inactive rendering for the frame in the window the select
  // is opened within if necessary.
  virtual void RenderWidgetShowing() {}
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_DELEGATE_H_
