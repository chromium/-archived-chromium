// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FIND_BAR_WIN_H_
#define CHROME_BROWSER_VIEWS_FIND_BAR_WIN_H_

#include "app/animation.h"
#include "base/gfx/rect.h"
#include "base/gfx/native_widget_types.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/find_bar.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "views/focus/focus_manager.h"

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

// TODO(sky): rename this to FindBarViews.

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
class FindBarWin : public views::AcceleratorTarget,
                   public views::FocusChangeListener,
                   public AnimationDelegate,
                   public FindBar,
                   public FindBarTesting {
 public:
  explicit FindBarWin(BrowserView* browser_view);
  virtual ~FindBarWin();

  // Whether we are animating the position of the Find window.
  bool IsAnimating();

#if defined(OS_WIN)
  // Forwards selected keystrokes to the renderer. This is useful to make sure
  // that arrow keys and PageUp and PageDown result in scrolling, instead of
  // being eaten because the FindBar has focus. Returns true if the keystroke
  // was forwarded, false if not.
  bool MaybeForwardKeystrokeToWebpage(UINT message, TCHAR key, UINT flags);
#endif

  void OnFinalMessage();

  bool IsVisible();

  // FindBar implementation:
  virtual FindBarController* GetFindBarController() const {
    return find_bar_controller_;
  }
  virtual void SetFindBarController(FindBarController* find_bar_controller) {
    find_bar_controller_ = find_bar_controller;
  }
  virtual void Show();
  virtual void Hide(bool animate);
  virtual void SetFocusAndSelection();
  virtual void ClearResults(const FindNotificationDetails& results);
  virtual void StopAnimation();
  virtual void MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                     bool no_redraw);
  virtual void SetFindText(const string16& find_text);
  virtual void UpdateUIForFindResult(const FindNotificationDetails& result,
                                     const string16& find_text);
  virtual void AudibleAlert();
  virtual gfx::Rect GetDialogPosition(gfx::Rect avoid_overlapping_rect);
  virtual void SetDialogPosition(const gfx::Rect& new_pos, bool no_redraw);
  virtual bool IsFindBarVisible();
  virtual void RestoreSavedFocus();
  virtual FindBarTesting* GetFindBarTesting();

  // Overridden from views::FocusChangeListener:
  virtual void FocusWillChange(views::View* focused_before,
                               views::View* focused_now);

  // Overridden from views::AcceleratorTarget:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // AnimationDelegate implementation:
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // FindBarTesting implementation:
  virtual bool GetFindBarWindowInfo(gfx::Point* position,
                                    bool* fully_visible);

  // Get the offset with which to paint the theme image.
  void GetThemePosition(gfx::Rect* bounds);

  // During testing we can disable animations by setting this flag to true,
  // so that opening and closing the Find box happens instantly, instead of
  // having to poll it while it animates to open/closed status.
  static bool disable_animations_during_testing_;

 private:
  class Host;

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

  // True if the accelerator target for Esc key is registered.
  bool esc_accel_target_registered_;

  // Tracks and stores the last focused view which is not the FindBarView
  // or any of its children. Used to restore focus once the FindBarView is
  // closed.
  scoped_ptr<views::ExternalFocusTracker> focus_tracker_;

  // A pointer back to the owning controller.
  FindBarController* find_bar_controller_;

  // Host is the Widget implementation that is created and maintained by the
  // find bar. It contains the FindBarView.
  scoped_ptr<Host> host_;

  DISALLOW_COPY_AND_ASSIGN(FindBarWin);
};

#endif  // CHROME_BROWSER_VIEWS_FIND_BAR_WIN_H_
