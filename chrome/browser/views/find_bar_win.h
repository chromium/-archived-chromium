// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FIND_BAR_WIN_H_
#define CHROME_BROWSER_VIEWS_FIND_BAR_WIN_H_

#include "base/gfx/rect.h"
#include "chrome/browser/find_bar.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/common/animation.h"
#include "chrome/views/widget_win.h"

class BrowserView;
class FindBarController;
class FindBarView;
class FindNotificationDetails;
class RenderViewHost;
class SlideAnimation;

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
// receives notifications about the search results and communicates that to the
// view.
//
// There is one FindBarWin per BrowserView, and its state is updated whenever
// the selected Tab is changed. The FindBarWin is created when the BrowserView
// is attached to the frame's Widget for the first time.
//
////////////////////////////////////////////////////////////////////////////////
class FindBarWin : public views::FocusChangeListener,
                   public views::WidgetWin,
                   public AnimationDelegate,
                   public FindBar {
 public:
  explicit FindBarWin(BrowserView* browser_view);
  virtual ~FindBarWin();

  // Accessor for find_bar_controller so FindBarView can get back to
  // FindBarController.
  FindBarController* find_bar_controller() const {
    return find_bar_controller_;
  }
  void set_find_bar_controller(FindBarController* find_bar_controller) {
    find_bar_controller_ = find_bar_controller;
  }

  // If the find bar obscures the search results we need to move the window. To
  // do that we need to know what is selected on the page. We simply calculate
  // where it would be if we place it on the left of the selection and if it
  // doesn't fit on the screen we try the right side. The parameter
  // |selection_rect| is expected to have coordinates relative to the top of
  // the web page area. If |no_redraw| is true, the window will be moved without
  // redrawing siblings.
  void MoveWindowIfNecessary(const gfx::Rect& selection_rect, bool no_redraw);

  // Moves the window according to the new window size.
  void RespondToResize(const gfx::Size& new_size);

  // Whether we are animating the position of the Find window.
  bool IsAnimating();

  // We need to monitor focus changes so that we can register a handler for
  // Escape when we get focus and unregister it when we looses focus. This
  // function unregisters our old Escape accelerator (if registered) and
  // registers a new one with the FocusManager associated with the
  // new |parent_hwnd|.
  void SetFocusChangeListener(HWND parent_hwnd);

  // FindBar implementation:
  virtual void Show();
  virtual void Hide(bool animate);
  virtual void SetFocusAndSelection();
  virtual void ClearResults(const FindNotificationDetails& results);
  virtual void StopAnimation();
  virtual void SetFindText(const std::wstring& find_text);
  virtual void UpdateUIForFindResult(const FindNotificationDetails& result,
                                     const std::wstring& find_text);
  virtual gfx::Rect GetDialogPosition(gfx::Rect avoid_overlapping_rect);
  virtual void SetDialogPosition(const gfx::Rect& new_pos, bool no_redraw);
  virtual bool IsFindBarVisible();
  virtual void RestoreSavedFocus();

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

  // The dialog needs rounded edges, so we create a polygon that corresponds to
  // the background images for this window (and make the polygon only contain
  // the pixels that we want to draw). The polygon is then given to SetWindowRgn
  // which changes the window from being a rectangle in shape, to being a rect
  // with curved edges. We also check to see if the region should be truncated
  // to prevent from drawing onto Chrome's window border.
  void UpdateWindowEdges(const gfx::Rect& new_pos);


  // Registers this class as the handler for when Escape is pressed. We will
  // unregister once we loose focus. See also: SetFocusChangeListener().
  void RegisterEscAccelerator();

  // When we loose focus, we unregister the handler for Escape. See
  // also: SetFocusChangeListener().
  void UnregisterEscAccelerator();

  // The BrowserView that created us.
  BrowserView* browser_view_;

  // Our view, which is responsible for drawing the UI.
  FindBarView* view_;

  // The y position pixel offset of the window while animating the Find dialog.
  int find_dialog_animation_offset_;

  // The animation class to use when opening the Find window.
  scoped_ptr<SlideAnimation> animation_;

  // The focus manager we register with to keep track of focus changes.
  views::FocusManager* focus_manager_;

  // Stores the previous accelerator target for the Escape key, so that we can
  // restore the state once we loose focus.
  views::AcceleratorTarget* old_accel_target_for_esc_;

  // Tracks and stores the last focused view which is not the FindBarView
  // or any of its children. Used to restore focus once the FindBarView is
  // closed.
  scoped_ptr<views::ExternalFocusTracker> focus_tracker_;

  // A pointer back to the owning controller.
  FindBarController* find_bar_controller_;

  DISALLOW_COPY_AND_ASSIGN(FindBarWin);
};

#endif  // CHROME_BROWSER_VIEWS_FIND_BAR_WIN_H_
