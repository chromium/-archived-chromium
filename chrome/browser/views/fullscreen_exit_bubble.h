// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FULLSCREEN_EXIT_BUBBLE_H__
#define CHROME_BROWSER_VIEWS_FULLSCREEN_EXIT_BUBBLE_H__

#include "base/scoped_ptr.h"
#include "chrome/browser/command_updater.h"
#include "chrome/common/slide_animation.h"
#include "chrome/views/controls/link.h"
#include "chrome/views/widget/widget_win.h"

// FullscreenExitBubble is responsible for showing a bubble atop the screen in
// fullscreen mode, telling users how to exit and providing a click target.
// The bubble auto-hides, and re-shows when the user moves to the screen top.

class FullscreenExitBubble : public views::LinkController,
                             public AnimationDelegate {
 public:
  explicit FullscreenExitBubble(
      views::Widget* frame,
      CommandUpdater::CommandUpdaterDelegate* delegate);
  virtual ~FullscreenExitBubble();

 private:
  class FullscreenExitView;
  class FullscreenExitPopup;

  static const double kOpacity;          // Opacity of the bubble, 0.0 - 1.0
  static const int kInitialDelayMs;      // Initial time bubble remains onscreen
  static const int kPositionCheckHz;     // How fast to check the mouse position
  static const int kSlideInRegionHeightPx;
                                         // Height of region triggering slide-in
  static const int kSlideInDurationMs;   // Duration of slide-in animation
  static const int kSlideOutDurationMs;  // Duration of slide-out animation

  // views::LinkController
  virtual void LinkActivated(views::Link* source, int event_flags);

  // AnimationDelegate
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // Called after the initial delay to start checking the mouse position.
  void AfterInitialDelay();

  // Called repeatedly to get the current mouse position and animate the bubble
  // on or off the screen as appropriate.
  void CheckMousePosition();

  // Returns the current desirable rect for the popup window.  If
  // |ignore_animation_state| is true this returns the rect assuming the popup
  // is fully onscreen.
  gfx::Rect GetPopupRect(bool ignore_animation_state) const;

  // The root view containing us.
  views::View* root_view_;

  // Someone who can toggle fullscreen mode on and off when the user requests
  // it.
  CommandUpdater::CommandUpdaterDelegate* delegate_;

  // The popup itself, which is a slightly modified WidgetWin.  We need to use
  // a WidgetWin (and thus an HWND) to make the popup float over other HWNDs.
  FullscreenExitPopup* popup_;

  // The contents of the popup.
  FullscreenExitView* view_;

  // Animation controlling sliding into/out of the top of the screen.
  scoped_ptr<SlideAnimation> size_animation_;

  // Timer to delay before starting the mouse checking/bubble hiding code.
  base::OneShotTimer<FullscreenExitBubble> initial_delay_;

  // Timer to poll the current mouse position.  We can't just listen for mouse
  // events without putting a non-empty HWND onscreen (or hooking Windows, which
  // has other problems), so instead we run a low-frequency poller to see if the
  // user has moved in or out of our show/hide regions.
  base::RepeatingTimer<FullscreenExitBubble> mouse_position_checker_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenExitBubble);
};

#endif  // CHROME_BROWSER_VIEWS_FULLSCREEN_EXIT_BUBBLE_H__
