// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_WINDOW_H_
#define CHROME_BROWSER_BROWSER_WINDOW_H_

#include <map>

#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "chrome/views/accelerator.h"

class BookmarkBarView;
class Browser;
class BrowserList;
class BrowserView;
class GoButton;
class LocationBarView;
class Profile;
class StatusBubble;
class TabContents;
class TabStrip;
class ToolbarStarToggle;
namespace views {
class RootView;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow interface
//  An interface implemented by the "view" of the Browser window.
//
class BrowserWindow {
 public:
  // Initialize the frame.
  virtual void Init() = 0;

  // Show the window according to the given command, which is one of SW_*
  // passed to the Windows function ShowWindow.
  //
  // If adjust_to_fit is true, the window is resized and moved to be on the
  // default screen.
  virtual void Show(int command, bool adjust_to_fit) = 0;

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

  // Updates internal state specifying whether the throbber is to be shown.
  // If the throbber was shown, and should still be shown, the frame of the
  // throbber is advanced.
  // If necessary, the appropriate painting is scheduled.
  virtual void ValidateThrobber() { }

  // TODO(beng): RENAME (GetRestoredBounds)
  // Returns the nonmaximized bounds of the frame (even if the frame is
  // currently maximized or minimized) in terms of the screen coordinates.
  virtual gfx::Rect GetNormalBounds() = 0;

  // TODO(beng): REMOVE?
  // Returns true if the frame is maximized (aka zoomed).
  virtual bool IsMaximized() = 0;

  // Returns the star button.
  virtual ToolbarStarToggle* GetStarButton() const = 0;

  // Returns the location bar.
  virtual LocationBarView* GetLocationBarView() const = 0;

  // Returns the go button.
  virtual GoButton* GetGoButton() const = 0;

  // Returns the Bookmark Bar view.
  virtual BookmarkBarView* GetBookmarkBarView() = 0;

  // Updates the toolbar with the state for the specified |contents|.
  virtual void UpdateToolbar(TabContents* contents,
                             bool should_restore_state) = 0;

  // Focuses the toolbar (for accessibility).
  virtual void FocusToolbar() = 0;

  // Returns whether the bookmark bar is visible or not.
  virtual bool IsBookmarkBarVisible() const = 0;

  // Construct a BrowserWindow implementation for the specified |browser|.
  static BrowserWindow* CreateBrowserWindow(Browser* browser,
                                            const gfx::Rect& bounds,
                                            int show_command);

 protected:
  friend class BrowserList;
  friend class BrowserView;
  virtual void DestroyBrowser() = 0;
};

#endif  // CHROME_BROWSER_BROWSER_WINDOW_H__
