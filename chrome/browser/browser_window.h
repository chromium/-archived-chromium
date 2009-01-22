// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_WINDOW_H_
#define CHROME_BROWSER_BROWSER_WINDOW_H_

class BookmarkBarView;
class Browser;
class BrowserList;
class BrowserView;
class BrowserWindowTesting;
class GoButton;
class LocationBarView;
class HtmlDialogContentsDelegate;
class Profile;
class StatusBubble;
class TabContents;
class TabStrip;
class ToolbarStarToggle;

namespace gfx {
class Rect;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow interface
//  An interface implemented by the "view" of the Browser window.
//
// NOTE: all getters, save GetTabStrip(), may return NULL.
class BrowserWindow {
 public:
  // Initialize the frame.
  virtual void Init() = 0;

  // Show the window, or activates it if it's already visible.
  virtual void Show() = 0;

  // Sets the window's size and position to the specified values.
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  // Closes the frame as soon as possible.  If the frame is not in a drag
  // session, it will close immediately; otherwise, it will move offscreen (so
  // events are still fired) until the drag ends, then close.
  virtual void Close() = 0;

  // Activates (brings to front) the window. Restores the window from minimized
  // state if necessary.
  virtual void Activate() = 0;

  // Flashes the taskbar item associated with this frame.
  virtual void FlashFrame() = 0;

  // Return a platform dependent identifier for this frame. On Windows, this
  // returns an HWND. DO NOT USE IN CROSS PLATFORM CODE.
  virtual void* GetNativeHandle() = 0;

  // Returns a pointer to the testing interface to the Browser window, or NULL
  // if there is none.
  virtual BrowserWindowTesting* GetBrowserWindowTesting() = 0;

  // TODO(beng): REMOVE (obtain via BrowserFrame).
  // Return the TabStrip associated with the frame.
  virtual TabStrip* GetTabStrip() const = 0;

  // Return the status bubble associated with the frame
  virtual StatusBubble* GetStatusBubble() = 0;

  // Inform the receiving frame that an animation has progressed in the
  // selected tab.
  // TODO(beng): Remove. Infobars/Boomarks bars should talk directly to
  //             BrowserView.
  virtual void SelectedTabToolbarSizeChanged(bool is_animating) = 0;

  // Inform the frame that the selected tab favicon or title has changed. Some
  // frames may need to refresh their title bar.
  // TODO(beng): make this pure virtual after XPFrame/VistaFrame retire.
  virtual void UpdateTitleBar() = 0;

  // Update any loading animations running in the window. |should_animate| is
  // true if there are tabs loading and the animations should continue, false
  // if there are no active loads and the animations should end.
  virtual void UpdateLoadingAnimations(bool should_animate) = 0;

  // TODO(beng): RENAME (GetRestoredBounds)
  // Returns the nonmaximized bounds of the frame (even if the frame is
  // currently maximized or minimized) in terms of the screen coordinates.
  virtual gfx::Rect GetNormalBounds() const = 0;

  // TODO(beng): REMOVE?
  // Returns true if the frame is maximized (aka zoomed).
  virtual bool IsMaximized() = 0;

  // Returns the star button.
  virtual ToolbarStarToggle* GetStarButton() const = 0;

  // Returns the location bar.
  virtual LocationBarView* GetLocationBarView() const = 0;

  // Returns the go button.
  virtual GoButton* GetGoButton() const = 0;

  // Updates the toolbar with the state for the specified |contents|.
  virtual void UpdateToolbar(TabContents* contents,
                             bool should_restore_state) = 0;

  // Focuses the toolbar (for accessibility).
  virtual void FocusToolbar() = 0;

  // Returns whether the bookmark bar is visible or not.
  virtual bool IsBookmarkBarVisible() const = 0;

  // Shows or hides the bookmark bar depending on its current visibility.
  virtual void ToggleBookmarkBar() = 0;

  // Shows the About Chrome dialog box.
  virtual void ShowAboutChromeDialog() = 0;

  // Shows the Bookmark Manager window.
  virtual void ShowBookmarkManager() = 0;

  // Shows the Report a Bug dialog box.
  virtual void ShowReportBugDialog() = 0;

  // Shows the Clear Browsing Data dialog box.
  virtual void ShowClearBrowsingDataDialog() = 0;

  // Shows the Import Bookmarks & Settings dialog box.
  virtual void ShowImportDialog() = 0;
  
  // Shows the Search Engines dialog box.
  virtual void ShowSearchEnginesDialog() = 0;

  // Shows the Password Manager dialog box.
  virtual void ShowPasswordManager() = 0;

  // Shows a dialog box with HTML content, e.g. for Gears. |parent_window| is
  // the window the dialog should be opened modal to and is a native window
  // handle.
  virtual void ShowHTMLDialog(HtmlDialogContentsDelegate* delegate,
                              void* parent_window) = 0;

  // Construct a BrowserWindow implementation for the specified |browser|.
  static BrowserWindow* CreateBrowserWindow(Browser* browser);

 protected:
  friend class BrowserList;
  friend class BrowserView;
  virtual void DestroyBrowser() = 0;
};

// A BrowserWindow utility interface used for accessing elements of the browser
// UI used only by UI test automation.
class BrowserWindowTesting {
public:
#if defined(OS_WIN)
  // Returns the Bookmark Bar view.
  virtual BookmarkBarView* GetBookmarkBarView() = 0;
#endif
};

#endif  // CHROME_BROWSER_BROWSER_WINDOW_H__
