// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_WINDOW_H_
#define CHROME_BROWSER_BROWSER_WINDOW_H_

#include "base/gfx/native_widget_types.h"

class Browser;
class BrowserWindowTesting;
class DownloadShelf;
class FindBar;
class GURL;
class HtmlDialogUIDelegate;
class LocationBar;
class Profile;
class StatusBubble;
class TabContents;
class TabContentsContainer;
class TemplateURL;

namespace gfx {
class Rect;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow interface
//  An interface implemented by the "view" of the Browser window.
//
// NOTE: All getters except GetTabStrip() may return NULL.
class BrowserWindow {
 public:
  // Show the window, or activates it if it's already visible.
  virtual void Show() = 0;

  // Sets the window's size and position to the specified values.
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  // Closes the frame as soon as possible.  If the frame is not in a drag
  // session, it will close immediately; otherwise, it will move offscreen (so
  // events are still fired) until the drag ends, then close. This assumes
  // that the Browser is not immediately destroyed, but will be eventually
  // destroyed by other means (eg, the tab strip going to zero elements).
  // Bad things happen if the Browser dtor is called directly as a result of
  // invoking this method.
  virtual void Close() = 0;

  // Activates (brings to front) the window. Restores the window from minimized
  // state if necessary.
  virtual void Activate() = 0;

  // Returns true if the window is currently the active/focused window.
  virtual bool IsActive() const = 0;

  // Flashes the taskbar item associated with this frame.
  virtual void FlashFrame() = 0;

  // Return a platform dependent identifier for this frame. On Windows, this
  // returns an HWND.
  virtual gfx::NativeWindow GetNativeHandle() = 0;

  // Returns a pointer to the testing interface to the Browser window, or NULL
  // if there is none.
  virtual BrowserWindowTesting* GetBrowserWindowTesting() = 0;

  // Return the status bubble associated with the frame
  virtual StatusBubble* GetStatusBubble() = 0;

  // Inform the receiving frame that an animation has progressed in the
  // selected tab.
  // TODO(beng): Remove. Infobars/Boomarks bars should talk directly to
  //             BrowserView.
  virtual void SelectedTabToolbarSizeChanged(bool is_animating) = 0;

  // Inform the frame that the selected tab favicon or title has changed. Some
  // frames may need to refresh their title bar.
  virtual void UpdateTitleBar() = 0;

  // Inform the frame that the dev tools window for the selected tab has
  // changed.
  virtual void UpdateDevTools() = 0;

  // Update any loading animations running in the window. |should_animate| is
  // true if there are tabs loading and the animations should continue, false
  // if there are no active loads and the animations should end.
  virtual void UpdateLoadingAnimations(bool should_animate) = 0;

  // Sets the starred state for the current tab.
  virtual void SetStarredState(bool is_starred) = 0;

  // TODO(beng): RENAME (GetRestoredBounds)
  // Returns the nonmaximized bounds of the frame (even if the frame is
  // currently maximized or minimized) in terms of the screen coordinates.
  virtual gfx::Rect GetNormalBounds() const = 0;

  // TODO(beng): REMOVE?
  // Returns true if the frame is maximized (aka zoomed).
  virtual bool IsMaximized() const = 0;

  // Accessors for fullscreen mode state.
  virtual void SetFullscreen(bool fullscreen) = 0;
  virtual bool IsFullscreen() const = 0;

  // Returns the location bar.
  virtual LocationBar* GetLocationBar() const = 0;

  // Tries to focus the location bar.  Clears the window focus (to avoid
  // inconsistent state) if this fails.
  virtual void SetFocusToLocationBar() = 0;

  // Informs the view whether or not a load is in progress for the current tab.
  // The view can use this notification to update the go/stop button.
  virtual void UpdateStopGoState(bool is_loading, bool force) = 0;

  // Updates the toolbar with the state for the specified |contents|.
  virtual void UpdateToolbar(TabContents* contents,
                             bool should_restore_state) = 0;

  // Focuses the toolbar (for accessibility).
  virtual void FocusToolbar() = 0;

  // Returns whether the bookmark bar is visible or not.
  virtual bool IsBookmarkBarVisible() const = 0;

  // Returns the rect where the resize corner should be drawn by the render
  // widget host view (on top of what the renderer returns). We return an empty
  // rect to identify that there shouldn't be a resize corner (in the cases
  // where we take care of it ourselves at the browser level).
  virtual gfx::Rect GetRootWindowResizerRect() const = 0;

  // Tells the frame not to render as inactive until the next activation change.
  // This is required on Windows when dropdown selects are shown to prevent the
  // select from deactivating the browser frame. A stub implementation is
  // provided here since the functionality is Windows-specific.
  virtual void DisableInactiveFrame() {}

  // Shows a confirmation dialog box for adding a search engine described by
  // |template_url|.
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                        Profile* profile) = 0;

  // Shows or hides the bookmark bar depending on its current visibility.
  virtual void ToggleBookmarkBar() = 0;

  // Shows the About Chrome dialog box.
  virtual void ShowAboutChromeDialog() = 0;

  // Shows the Task manager.
  virtual void ShowTaskManager() = 0;

  // Shows the Bookmark Manager window.
  virtual void ShowBookmarkManager() = 0;

  // Shows the Bookmark bubble. |url| is the URL being bookmarked,
  // |already_bookmarked| is true if the url is already bookmarked.
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) = 0;

  // Whether or not the shelf view is visible.
  virtual bool IsDownloadShelfVisible() const = 0;

  // Returns the DownloadShelf.
  virtual DownloadShelf* GetDownloadShelf() = 0;

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

  // Shows the Select Profile dialog box.
  virtual void ShowSelectProfileDialog() = 0;

  // Shows the New Profile dialog box.
  virtual void ShowNewProfileDialog() = 0;

  // Shows the confirmation dialog box warning that the browser is closing with
  // in-progress downloads.
  // This method should call Browser::InProgressDownloadResponse once the user
  // has confirmed.
  virtual void ConfirmBrowserCloseWithPendingDownloads() = 0;

  // Shows a dialog box with HTML content, e.g. for Gears. |parent_window| is
  // the window the dialog should be opened modal to and is a native window
  // handle.
  virtual void ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                              gfx::NativeWindow parent_window) = 0;

  // Asks the window to continue a drag operation begun in a different browser
  // window. |tab_bounds| are the bounds of the Tab view that was dragged from
  // the source window, in screen coordinates. The corresponding Tab view in
  // this new window will be positioned at these bounds for a seamless
  // appearance.
  virtual void ContinueDraggingDetachedTab(const gfx::Rect& tab_bounds) {}

  // BrowserThemeProvider calls this when a user has changed his or her theme,
  // indicating that it's time to redraw everything.
  virtual void UserChangedTheme() = 0;

  // Get extra vertical height that the render view should add to its requests
  // to webkit. This can help prevent sending extraneous layout/repaint requests
  // when the delegate is in the process of resizing the tab contents view (e.g.
  // during infobar animations).
  virtual int GetExtraRenderViewHeight() const = 0;

  // Notification that |tab_contents| got the focus through user action (click
  // on the page).
  virtual void TabContentsFocused(TabContents* tab_contents) = 0;

  // Construct a BrowserWindow implementation for the specified |browser|.
  static BrowserWindow* CreateBrowserWindow(Browser* browser);

  // Construct a FindBar implementation for the specified |browser|.
  static FindBar* CreateFindBar(Browser* browser_window);

 protected:
  friend class BrowserList;
  friend class BrowserView;
  virtual void DestroyBrowser() = 0;
};

#if defined(OS_WIN)
class BookmarkBarView;
class LocationBarView;

namespace views {
class View;
}
#endif  // defined(OS_WIN)

// A BrowserWindow utility interface used for accessing elements of the browser
// UI used only by UI test automation.
class BrowserWindowTesting {
 public:
#if defined(OS_WIN)
  // Returns the BookmarkBarView.
  virtual BookmarkBarView* GetBookmarkBarView() const = 0;

  // Returns the LocationBarView.
  virtual LocationBarView* GetLocationBarView() const = 0;

  // Returns the TabContentsContainer.
  virtual views::View* GetTabContentsContainerView() const = 0;
#endif
};

#endif  // CHROME_BROWSER_BROWSER_WINDOW_H_
