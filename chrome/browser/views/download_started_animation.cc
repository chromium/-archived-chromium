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

#include "chrome/browser/views/download_started_animation.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/hwnd_view_container.h"

// How long to spend moving downwards and fading out after waiting.
static const int kMoveTimeMs = 600;

// The animation framerate.
static const int kFrameRateHz = 60;

// What fraction of the frame height to move downward from the frame center.
// Note that setting this greater than 0.5 will mean moving past the bottom of
// the frame.
static const double kMoveFraction = 1.0 / 3.0;

DownloadStartedAnimation::DownloadStartedAnimation(TabContents* tab_contents)
    : Animation(kMoveTimeMs, kFrameRateHz, NULL),
      popup_(NULL),
      tab_contents_(tab_contents) {
  static SkBitmap* kDownloadImage = NULL;
  if (!kDownloadImage) {
    kDownloadImage = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_DOWNLOAD_ANIMATION_BEGIN);
  }

  // If we're too small to show the download image, then don't bother -
  // the shelf will be enough.
  tab_contents_->GetContainerBounds(&tab_contents_bounds_);
  if (tab_contents_bounds_.height() < kDownloadImage->height())
    return;

  NotificationService::current()->AddObserver(this, NOTIFY_TAB_CONTENTS_HIDDEN,
      Source<TabContents>(tab_contents_));
  NotificationService::current()->AddObserver(this,
      NOTIFY_TAB_CONTENTS_DESTROYED, Source<TabContents>(tab_contents_));

  SetImage(kDownloadImage);

  gfx::Rect rc(0, 0, 0, 0);
  popup_ = new ChromeViews::HWNDViewContainer;
  popup_->set_window_style(WS_POPUP);
  popup_->set_window_ex_style(WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                              WS_EX_TRANSPARENT);
  popup_->SetLayeredAlpha(0x00);
  popup_->Init(tab_contents_->GetContainerHWND(), rc, this, false);
  Reposition();
  popup_->ShowWindow(SW_SHOWNOACTIVATE);

  Start();
}

void DownloadStartedAnimation::Reposition() {
  if (!tab_contents_)
    return;

  // Align the image with the bottom left of the web contents (so that it
  // points to the newly created download).
  CSize size;
  GetPreferredSize(&size);
  popup_->MoveWindow(tab_contents_bounds_.x(),
                     static_cast<int>(tab_contents_bounds_.bottom() - size.cy -
                     size.cy * (1 - GetCurrentValue())),
                     size.cx, size.cy);
}

void DownloadStartedAnimation::Close() {
  if (!tab_contents_)
    return;

  NotificationService::current()->RemoveObserver(this,
      NOTIFY_TAB_CONTENTS_HIDDEN, Source<TabContents>(tab_contents_));
  NotificationService::current()->RemoveObserver(this,
      NOTIFY_TAB_CONTENTS_DESTROYED, Source<TabContents>(tab_contents_));
  tab_contents_ = NULL;
  popup_->Close();
}

void DownloadStartedAnimation::AnimateToState(double state) {
  if (state >= 1.0) {
    Close();
  } else {
    Reposition();

    // Start at zero, peak halfway and end at zero.
    double opacity = std::min(1.0 - pow(GetCurrentValue() - 0.5, 2) * 4.0,
                              static_cast<double>(1.0));

    popup_->SetLayeredAlpha(
        static_cast<BYTE>(opacity * 255.0));
    SchedulePaint();  // Reposition() calls MoveWindow() which never picks up
                      // alpha changes, so we need to force a paint.
  }
}

void DownloadStartedAnimation::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  Close();
}
