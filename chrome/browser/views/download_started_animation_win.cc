// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_started_animation.h"

#include "app/animation.h"
#include "app/resource_bundle.h"
#include "base/gfx/rect.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "grit/theme_resources.h"
#include "views/controls/image_view.h"
#include "views/widget/widget_win.h"

// How long to spend moving downwards and fading out after waiting.
static const int kMoveTimeMs = 600;

// The animation framerate.
static const int kFrameRateHz = 60;

// What fraction of the frame height to move downward from the frame center.
// Note that setting this greater than 0.5 will mean moving past the bottom of
// the frame.
static const double kMoveFraction = 1.0 / 3.0;

namespace {

// DownloadStartAnimation creates an animation (which begins running
// immediately) that animates an image downward from the center of the frame
// provided on the constructor, while simultaneously fading it out.  To use,
// simply call "new DownloadStartAnimation"; the class cleans itself up when it
// finishes animating.
class DownloadStartedAnimationWin : public Animation,
                                    public NotificationObserver,
                                    public views::ImageView {
 public:
  DownloadStartedAnimationWin(TabContents* tab_contents);

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

  // A scoped container for notification registries.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DownloadStartedAnimationWin);
};

DownloadStartedAnimationWin::DownloadStartedAnimationWin(
    TabContents* tab_contents)
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

  registrar_.Add(
      this,
      NotificationType::TAB_CONTENTS_HIDDEN,
      Source<TabContents>(tab_contents_));
  registrar_.Add(
      this,
      NotificationType::TAB_CONTENTS_DESTROYED,
      Source<TabContents>(tab_contents_));

  SetImage(kDownloadImage);

  gfx::Rect rc(0, 0, 0, 0);
  popup_ = new views::WidgetWin;
  popup_->set_window_style(WS_POPUP);
  popup_->set_window_ex_style(WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                              WS_EX_TRANSPARENT);
  popup_->SetOpacity(0x00);
  popup_->Init(tab_contents_->GetNativeView(), rc);
  popup_->SetContentsView(this);
  Reposition();
  popup_->Show();

  Start();
}

void DownloadStartedAnimationWin::Reposition() {
  if (!tab_contents_)
    return;

  // Align the image with the bottom left of the web contents (so that it
  // points to the newly created download).
  gfx::Size size = GetPreferredSize();
  int x = UILayoutIsRightToLeft() ?
      tab_contents_bounds_.right() - size.width() : tab_contents_bounds_.x();
  popup_->MoveWindow(
      x,
      static_cast<int>(tab_contents_bounds_.bottom() -
          size.height() - size.height() * (1 - GetCurrentValue())),
      size.width(),
      size.height());
}

void DownloadStartedAnimationWin::Close() {
  if (!tab_contents_)
    return;

  registrar_.Remove(
      this,
      NotificationType::TAB_CONTENTS_HIDDEN,
      Source<TabContents>(tab_contents_));
  registrar_.Remove(
      this,
      NotificationType::TAB_CONTENTS_DESTROYED,
      Source<TabContents>(tab_contents_));
  tab_contents_ = NULL;
  popup_->Close();
}

void DownloadStartedAnimationWin::AnimateToState(double state) {
  if (state >= 1.0) {
    Close();
  } else {
    Reposition();

    // Start at zero, peak halfway and end at zero.
    double opacity = std::min(1.0 - pow(GetCurrentValue() - 0.5, 2) * 4.0,
                              static_cast<double>(1.0));

    popup_->SetOpacity(
        static_cast<SkColor>(opacity * 255.0));
    SchedulePaint();  // Reposition() calls MoveWindow() which never picks up
                      // alpha changes, so we need to force a paint.
  }
}

void DownloadStartedAnimationWin::Observe(NotificationType type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  Close();
}

}  // namespace

// static
void DownloadStartedAnimation::Show(TabContents* tab_contents) {
  // The animation will delete itself when it's finished or when the tab
  // contents is hidden or destroyed.
  new DownloadStartedAnimationWin(tab_contents);
}
