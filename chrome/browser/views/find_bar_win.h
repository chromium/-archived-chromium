// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FIND_BAR_WIN_H_
#define CHROME_BROWSER_VIEWS_FIND_BAR_WIN_H_

#include "base/gfx/rect.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/common/animation.h"
#include "chrome/views/widget_win.h"

class FindBarView;
class RenderViewHost;
class SlideAnimation;
class WebContentsView;

namespace views {
class ExternalFocusTracker;
class View;
}

////////////////////////////////////////////////////////////////////////////////
//
// The FindBarWin implements the container window for the Windows find-in-page
// functionality. It uses the FindBarWin implementation to draw its content and
// is responsible for showing, hiding, closing, and moving the window if needed,
// for example if the window is obscuring the selection results. It also
// communicates with the parent_tab to instruct it to start searching for what
// the user selected and receives notifications about the search results and
// communicates that to the view.
//
// We create one container per tab and remember each search query per tab.
//
////////////////////////////////////////////////////////////////////////////////
class FindBarWin : public views::FocusChangeListener,
                   public views::WidgetWin,
                   public AnimationDelegate {
 public:
  FindBarWin(WebContentsView* parent_tab, HWND parent_hwnd);
  virtual ~FindBarWin();

  // Shows the find bar. Any previous search string will again be visible.
  void Show();

  // Ends the current session.
  void EndFindSession();

  // Closes the find bar window (calls Close on the Window Container).
  void Close();

  // This is triggered when the parent tab of the Find dialog becomes
  // unselected, at which point the find bar should become hidden. Otherwise,
  // we leave artifacts on the chrome when other tabs are visible. However, we
  // need to show the Find dialog again when the parent tab becomes selected
  // again, so we set a flag for that and show the window if we get a
  // DidBecomeSelected call.
  void DidBecomeUnselected();

  // If |show_on_tab_selection_| is true, then we show the dialog and clear the
  // flag, otherwise we do nothing.
  void DidBecomeSelected();

  // Starts the Find operation by calling StartFinding on the Tab. This function
  // can be called from the outside as a result of hot-keys, so it uses the
  // last remembered search string as specified with set_find_string(). This
  // function does not block while a search is in progress. The controller will
  // receive the results through the notification mechanism. See Observe(...)
  // for details.
  void StartFinding(bool forward_direction);

  // Stops the current Find operation. If |clear_selection| is true, it will
  // also clear the selection on the focused frame.
  void StopFinding(bool clear_selection);

  // If the find bar obscures the search results we need to move the window. To
  // do that we need to know what is selected on the page. We simply calculate
  // where it would be if we place it on the left of the selection and if it
  // doesn't fit on the screen we try the right side. The parameter
  // |selection_rect| is expected to have coordinates relative to the top of
  // the web page area.
  void MoveWindowIfNecessary(const gfx::Rect& selection_rect);

  // Moves the window according to the new window size.
  void RespondToResize(const gfx::Size& new_size);

  // Whether we are animating the position of the Find window.
  bool IsAnimating();

  // Changes the parent window for the find bar. If |new_parent| is already
  // the parent of this window then no action is taken.
  // |new_parent| can not be NULL.
  void SetParent(HWND new_parent);

  // We need to monitor focus changes so that we can register a handler for
  // Escape when we get focus and unregister it when we looses focus. This
  // function unregisters our old Escape accelerator (if registered) and
  // registers a new one with the FocusManager associated with the
  // new |parent_hwnd|.
  void SetFocusChangeListener(HWND parent_hwnd);

  // Accessors for specifying and retrieving the current search string.
  std::wstring find_string() { return find_string_; }
  void set_find_string(const std::wstring& find_string) {
    find_string_ = find_string;
  }

  // Updates the find bar with the latest results. This is called when the
  // renderer (through the RenderViewHostDelegate::View) finds more stuff. 
  void OnFindReply(int request_id,
                   int number_of_matches,
                   const gfx::Rect& selection_rect,
                   int active_match_ordinal,
                   bool final_update);

  // Overridden from views::WidgetWin:
  virtual void OnFinalMessage(HWND window);

  // Overridden from views::FocusChangeListener:
  virtual void FocusWillChange(views::View* focused_before,
                               views::View* focused_now);

  // Overridden from views::AcceleratorTarget:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // AnimationDelegate implementation:
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

 private:
  // Returns the RenderViewHost associated with the current tab.
  RenderViewHost* GetRenderViewHost() const;

  // Retrieves the boundaries that the find bar has to work with within the
  // Chrome frame window. The resulting rectangle will be a rectangle that
  // overlaps the bottom of the Chrome toolbar by one pixel (so we can create
  // the illusion that the find bar is part of the toolbar) and covers the page
  // area, except that we deflate the rect width by subtracting (from both
  // sides) the width of the toolbar and some extra pixels to account for the
  // width of the Chrome window borders. |bounds| is relative to the browser
  // window. If the function fails to determine the browser window/client area
  // rectangle or the rectangle for the page area then |bounds| will
  // be an empty rectangle.
  void GetDialogBounds(gfx::Rect* bounds);

  // Returns the rectangle representing where to position the find bar. It uses
  // GetDialogBounds and positions itself within that, either to the left (if an
  // InfoBar is present) or to the right (no InfoBar). If
  // |avoid_overlapping_rect| is specified, the return value will be a rectangle
  // located immediately to the left of |avoid_overlapping_rect|, as long as
  // there is enough room for the dialog to draw within the bounds. If not, the
  // dialog position returned will overlap |avoid_overlapping_rect|.
  // Note: |avoid_overlapping_rect| is expected to use coordinates relative to
  // the top of the page area, (it will be converted to coordinates relative to
  // the top of the browser window, when comparing against the dialog
  // coordinates). The returned value is relative to the browser window.
  gfx::Rect GetDialogPosition(gfx::Rect avoid_overlapping_rect);

  // Moves the dialog window to the provided location, moves it to top in the
  // z-order (HWND_TOP, not HWND_TOPMOST) and shows the window (if hidden).
  // It then calls UpdateWindowEdges to make sure we don't overwrite the Chrome
  // window border.
  void SetDialogPosition(const gfx::Rect& new_pos);

  // The dialog needs rounded edges, so we create a polygon that corresponds to
  // the background images for this window (and make the polygon only contain
  // the pixels that we want to draw). The polygon is then given to SetWindowRgn
  // which changes the window from being a rectangle in shape, to being a rect
  // with curved edges. We also check to see if the region should be truncated
  // to prevent from drawing onto Chrome's window border.
  void UpdateWindowEdges(const gfx::Rect& new_pos);

  // Upon dismissing the window, restore focus to the last focused view which is
  // not FindBarView or any of its children.
  void RestoreSavedFocus();

  // Registers this class as the handler for when Escape is pressed. We will
  // unregister once we loose focus. See also: SetFocusChangeListener().
  void RegisterEscAccelerator();

  // When we loose focus, we unregister the handler for Escape. See
  // also: SetFocusChangeListener().
  void UnregisterEscAccelerator();

  // The tab we are associated with.
  WebContentsView* parent_tab_;

  // The window handle of our parent window (the main Chrome window).
  HWND parent_hwnd_;

  // Our view, which is responsible for drawing the UI.
  FindBarView* view_;

  // Each time a search request comes in we assign it an id before passing it
  // over the IPC so that when the results come in we can evaluate whether we
  // still care about the results of the search (in some cases we don't because
  // the user has issued a new search).
  static int request_id_counter_;

  // This variable keeps track of what the most recent request id is.
  int current_request_id_;

  // The current string we are searching for.
  std::wstring find_string_;

  // The last string we searched for. This is used to figure out if this is a
  // Find or a FindNext operation (FindNext should not increase the request id).
  std::wstring last_find_string_;

  // The current window position (relative to parent).
  gfx::Rect curr_pos_relative_;

  // The current window size.
  gfx::Size window_size_;

  // The y position pixel offset of the window while animating the Find dialog.
  int find_dialog_animation_offset_;

  // The animation class to use when opening the Find window.
  scoped_ptr<SlideAnimation> animation_;

  // Whether to show the Find dialog when its tab becomes selected again.
  bool show_on_tab_selection_;

  // The focus manager we register with to keep track of focus changes.
  views::FocusManager* focus_manager_;

  // Stores the previous accelerator target for the Escape key, so that we can
  // restore the state once we loose focus.
  views::AcceleratorTarget* old_accel_target_for_esc_;

  // Tracks and stores the last focused view which is not the FindBarView
  // or any of its children. Used to restore focus once the FindBarView is
  // closed.
  scoped_ptr<views::ExternalFocusTracker> focus_tracker_;

  DISALLOW_COPY_AND_ASSIGN(FindBarWin);
};

#endif  // CHROME_BROWSER_VIEWS_FIND_BAR_WIN_H_
