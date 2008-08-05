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
namespace ChromeViews {
class RootView;
}
class GoButton;
class LocationBarView;
class Profile;
class StatusBubble;
class TabContents;
class TabStrip;
class ToolbarStarToggle;

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

  // TODO(beng): REMOVE
  // Invoked by the browser when painting occurred. This is called as a
  // result of calling Browser::Paint()
  // TODO(ACW) We really need a cross platform region class to replace
  // HRGN in our APIs
  virtual void BrowserDidPaint(HRGN region) = 0;

  // Closes the frame as soon as possible.  If the frame is not in a drag
  // session, it will close immediately; otherwise, it will move offscreen (so
  // events are still fired) until the drag ends, then close.
  virtual void Close() = 0;

  // Return a platform dependent identifier for this frame. On Windows, this
  // returns an HWND.
  virtual void* GetPlatformID() = 0;

  // TODO(beng): REMOVE (obtain via BrowserFrame).
  // Return the TabStrip associated with the frame.
  virtual TabStrip* GetTabStrip() const = 0;

  // Return the status bubble associated with the frame
  virtual StatusBubble* GetStatusBubble() = 0;

  // Return the RootView associated with the frame.
  virtual ChromeViews::RootView* GetRootView() = 0;

  // Inform the receiving frame that the visibility of one of the shelfs/bars
  // may have changed.
  virtual void ShelfVisibilityChanged() = 0;

  // Inform the receiving frame that an animation has progressed in the
  // selected tab.
  virtual void SelectedTabToolbarSizeChanged(bool is_animating) = 0;

  // Inform the frame that the selected tab favicon or title has changed. Some
  // frames may need to refresh their title bar.
  virtual void UpdateTitleBar() { }

  // Sets the title displayed in various places within the OS, such as the task
  // bar.
  virtual void SetWindowTitle(const std::wstring& title) = 0;

  // Activates (brings to front) the frame. Deminiaturize the frame if needed.
  virtual void Activate() = 0;

  // Flashes the taskbar item associated with this frame.
  virtual void FlashFrame() = 0;

  // Makes the specified TabContents visible. If there is another TabContents
  // visible presently, this method is responsible for hiding that TabContents
  // cleanly as well.
  virtual void ShowTabContents(TabContents* contents) = 0;

  // Continue a drag gesture that began with a constrained window. When the
  // user drags a constrained window such that their mouse pointer leaves the
  // bounds of the constraining HWND, the window is detached and the drag
  // gesture continues except for this top level frame.
  // |mouse_pt| is the position of the cursor in screen coordinates.
  // |frame_component| is the component returned by WM_NCHITTEST for |mouse_pt|
  // on the constrained window. This is passed to ensure we initiate the
  // correct action (move, resize, etc).
  virtual void ContinueDetachConstrainedWindowDrag(
      const gfx::Point& mouse_pt,
      int frame_component) = 0;

  // Sizes the frame to match the specified desired bounds for the contents.
  // |contents_bounds| are in screen coordinates.
  virtual void SizeToContents(const gfx::Rect& contents_bounds) = 0;

  // Set the accelerator table. This is called once after LoadAccelerators
  // has been called on the frame. The callee becomes the owner of the passed
  // map. The map associates accelerators with command ids.
  // Note if you are not calling FrameUtil::LoadAccelerators() on this frame,
  // this method is never invoked.
  virtual void SetAcceleratorTable(
      std::map<ChromeViews::Accelerator, int>* accelerator_table) = 0;

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

  // TODO(beng): REMOVE - this work should be done entirely in the frames.
  // Returns the bounds required to accomodate for some contents located at the
  // provided rectangle. The result is in whatever coordinate system used for
  // |content_rect|.
  virtual gfx::Rect GetBoundsForContentBounds(const gfx::Rect content_rect) = 0;

  // Tel this frame to detach from the web browser. The frame should no longer
  // notify the browser about anything.
  virtual void DetachFromBrowser() = 0;

  // Invoked by the InfoBubble when it is shown/hidden. XPFrame/VistaFrame use
  // this notification to make sure they render as active even though they are
  // not active while the bubble is shown.
  virtual void InfoBubbleShowing() = 0;
  // The implementation for this sends WM_NCACTIVATE with a value of FALSE for
  // the window. Subclasses that need to customize should be sure and invoke
  // this implementation too.
  virtual void InfoBubbleClosing() {
    // TODO(beng): (Cleanup) - move out of here!
    HWND hwnd = static_cast<HWND>(GetPlatformID());
    // The frame is really inactive, send notification now.
    DefWindowProc(hwnd, WM_NCACTIVATE, FALSE, 0);
  }

  // Returns the star button.
  virtual ToolbarStarToggle* GetStarButton() const = 0;

  // Returns the location bar.
  virtual LocationBarView* GetLocationBarView() const = 0;

  // Returns the go button.
  virtual GoButton* GetGoButton() const = 0;

  // Returns the Bookmark Bar view.
  virtual BookmarkBarView* GetBookmarkBarView() = 0;

  // Returns the BrowserView.
  // TODO(beng): remove this! temporary only!
  virtual BrowserView* GetBrowserView() const = 0;

  // Updates the toolbar with the state for the specified |contents|.
  virtual void Update(TabContents* contents, bool should_restore_state) = 0;

  // Updates the UI with the specified Profile.
  virtual void ProfileChanged(Profile* profile) = 0;

  // Focuses the toolbar (for accessibility).
  virtual void FocusToolbar() = 0;

  // Construct a BrowserWindow implementation for the specified |browser|.
  static BrowserWindow* CreateBrowserWindow(Browser* browser,
                                            const gfx::Rect& bounds,
                                            int show_command);

 protected:
  friend class BrowserList;
  friend class BrowserView;
  virtual void DestroyBrowser() = 0;
};

#endif  // #ifndef CHROME_BROWSER_BROWSER_WINDOW_H__
