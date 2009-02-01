// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_DOWNLOAD_STARTED_ANIMATION_H_
#define CHROME_BROWSER_VIEWS_DOWNLOAD_STARTED_ANIMATION_H_

#include "base/gfx/rect.h"
#include "chrome/common/animation.h"
#include "chrome/common/notification_observer.h"
#include "chrome/views/image_view.h"

namespace views {
class WidgetWin;
};
class TabContents;

// DownloadStartAnimation creates an animation (which begins running
// immediately) that animates an image downward from the center of the frame
// provided on the constructor, while simultaneously fading it out.  To use,
// simply call "new DownloadStartAnimation"; the class cleans itself up when it
// finishes animating.
class DownloadStartedAnimation : public Animation,
                                 public NotificationObserver,
                                 public views::ImageView {
 public:
  DownloadStartedAnimation(TabContents* tab_contents);

 private:
  // Move the animation to wherever it should currently be.
  void Reposition();

  // Shut down the animation cleanly.
  void Close();

  // Animation
  virtual void AnimateToState(double state);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // We use a HWND for the popup so that it may float above any HWNDs in our UI.
  views::WidgetWin* popup_;

  // The content area holding us.
  TabContents* tab_contents_;

  // The content area at the start of the animation. We store this so that the
  // download shelf's resizing of the content area doesn't cause the animation
  // to move around. This means that once started, the animation won't move
  // with the parent window, but it's so fast that this shouldn't cause too
  // much heartbreak.
  gfx::Rect tab_contents_bounds_;

  DISALLOW_COPY_AND_ASSIGN(DownloadStartedAnimation);
};

#endif  // CHROME_BROWSER_VIEWS_DOWNLOAD_STARTED_ANIMATION_H_
