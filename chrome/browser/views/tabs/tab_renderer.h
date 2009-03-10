// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_RENDERER_H__
#define CHROME_BROWSER_VIEWS_TABS_TAB_RENDERER_H__

#include "base/gfx/point.h"
#include "chrome/common/animation.h"
#include "chrome/common/slide_animation.h"
#include "chrome/common/throb_animation.h"
#include "chrome/views/button.h"
#include "chrome/views/menu.h"
#include "chrome/views/view.h"

class TabContents;

///////////////////////////////////////////////////////////////////////////////
//
// TabRenderer
//
//  A View that renders a Tab, either in a TabStrip or in a DraggedTabView.
//
///////////////////////////////////////////////////////////////////////////////
class TabRenderer : public views::View,
                    public AnimationDelegate {
 public:
  // Possible animation states.
  enum AnimationState {
    ANIMATION_NONE,
    ANIMATION_WAITING,
    ANIMATION_LOADING
  };

  TabRenderer();
  virtual ~TabRenderer();

  // Updates the data the Tab uses to render itself from the specified
  // TabContents.
  void UpdateData(TabContents* contents);

  // Updates the display to reflect the contents of this TabRenderer's model.
  void UpdateFromModel();

  // Returns true if the Tab is selected, false otherwise.
  virtual bool IsSelected() const;

  // Advance the Loading Animation to the next frame, or hide the animation if
  // the tab isn't loading.
  void ValidateLoadingAnimation(AnimationState animation_state);

  // Starts/Stops a pulse animation.
  void StartPulse();
  void StopPulse();

  // Returns the minimum possible size of a single unselected Tab.
  static gfx::Size GetMinimumUnselectedSize();
  // Returns the minimum possible size of a selected Tab. Selected tabs must
  // always show a close button and have a larger minimum size than unselected
  // tabs.
  static gfx::Size GetMinimumSelectedSize();
  // Returns the preferred size of a single Tab, assuming space is
  // available.
  static gfx::Size GetStandardSize();

  // Loads the images to be used for the tab background. Uses the images for
  // Vista if |use_vista_images| is true.
  static void LoadTabImages(bool use_vista_images);

 protected:
  views::Button* close_button() const { return close_button_; }
  const gfx::Rect& title_bounds() const { return title_bounds_; }

  // Returns the title of the Tab.
  std::wstring GetTitle() const;

 private:
  // Overridden from views::View:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual void ThemeChanged();

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationCanceled(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // Starts/Stops the crash animation.
  void StartCrashAnimation();
  void StopCrashAnimation();

  // Return true if the crash animation is currently running.
  bool IsPerformingCrashAnimation() const;

  // Set the temporary offset for the favicon. This is used during animation.
  void SetFavIconHidingOffset(int offset);

  void DisplayCrashedFavIcon();
  void ResetCrashedFavIcon();

  // Paint various portions of the Tab
  void PaintTabBackground(ChromeCanvas* canvas);
  void PaintInactiveTabBackground(ChromeCanvas* canvas);
  void PaintActiveTabBackground(ChromeCanvas* canvas);
  void PaintHoverTabBackground(ChromeCanvas* canvas, double opacity);
  void PaintLoadingAnimation(ChromeCanvas* canvas);

  // Returns the number of favicon-size elements that can fit in the tab's
  // current size.
  int IconCapacity() const;

  // Returns whether the Tab should display a favicon.
  bool ShouldShowIcon() const;

  // Returns whether the Tab should display a close button.
  bool ShouldShowCloseBox() const;

  // The bounds of various sections of the display.
  gfx::Rect favicon_bounds_;
  gfx::Rect download_icon_bounds_;
  gfx::Rect title_bounds_;

  // Current state of the animation.
  AnimationState animation_state_;

  // The current index into the Animation image strip.
  int animation_frame_;

  // Close Button.
  views::Button* close_button_;

  // Hover animation.
  scoped_ptr<SlideAnimation> hover_animation_;

  // Pulse animation.
  scoped_ptr<ThrobAnimation> pulse_animation_;

  // Model data. We store this here so that we don't need to ask the underlying
  // model, which is tricky since instances of this object can outlive the
  // corresponding objects in the underlying model.
  struct TabData {
    SkBitmap favicon;
    std::wstring title;
    bool loading;
    bool crashed;
    bool off_the_record;
    bool show_icon;
    bool show_download_icon;
  };
  TabData data_;

  struct TabImage {
    SkBitmap* image_l;
    SkBitmap* image_c;
    SkBitmap* image_r;
    int l_width;
    int r_width;
  };
  static TabImage tab_active;
  static TabImage tab_inactive;
  static TabImage tab_inactive_otr;
  static TabImage tab_hover;

  // Whether we're showing the icon. It is cached so that we can detect when it
  // changes and layout appropriately.
  bool showing_icon_;

  // Whether we are showing the download icon. Comes from the model.
  bool showing_download_icon_;

  // Whether we are showing the close button. It is cached so that we can
  // detect when it changes and layout appropriately.
  bool showing_close_button_;

  // The offset used to animate the favicon location.
  int fav_icon_hiding_offset_;

  // The animation object used to swap the favicon with the sad tab icon.
  class FavIconCrashAnimation;
  FavIconCrashAnimation* crash_animation_;

  bool should_display_crashed_favicon_;

  static void InitClass();
  static bool initialized_;

  DISALLOW_EVIL_CONSTRUCTORS(TabRenderer);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_RENDERER_H__
