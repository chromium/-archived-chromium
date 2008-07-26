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

#ifndef CHROME_BROWSER_VIEWS_DOWNLOAD_STARTED_ANIMATION_H__
#define CHROME_BROWSER_VIEWS_DOWNLOAD_STARTED_ANIMATION_H__

#include "base/gfx/rect.h"
#include "chrome/common/animation.h"
#include "chrome/common/notification_service.h"
#include "chrome/views/image_view.h"

namespace ChromeViews {
  class HWNDViewContainer;
};
class TabContents;

// DownloadStartAnimation creates an animation (which begins running
// immediately) that animates an image downward from the center of the frame
// provided on the constructor, while simultaneously fading it out.  To use,
// simply call "new DownloadStartAnimation"; the class cleans itself up when it
// finishes animating.
class DownloadStartedAnimation : public Animation,
                                 public NotificationObserver,
                                 public ChromeViews::ImageView {
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
  ChromeViews::HWNDViewContainer* popup_;

  // The content area holding us.
  TabContents* tab_contents_;

  // The content area at the start of the animation. We store this so that the
  // download shelf's resizing of the content area doesn't cause the animation
  // to move around. This means that once started, the animation won't move
  // with the parent window, but it's so fast that this shouldn't cause too
  // much heartbreak.
  gfx::Rect tab_contents_bounds_;

  DISALLOW_EVIL_CONSTRUCTORS(DownloadStartedAnimation);
};

#endif  // CHROME_BROWSER_VIEWS_DOWNLOAD_STARTED_ANIMATION_H__
