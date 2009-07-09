// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_2_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_2_H_

#include "app/animation.h"
#include "base/string16.h"
#include "views/controls/button/button.h"
#include "views/view.h"

namespace gfx {
class Canvas;
class Path;
};
class SkBitmap;
class SlideAnimation;
class Tab2;
class ThrobAnimation;
namespace views {
class AnimationContext;
class Animator;
class AnimatorDelegate;
class ImageButton;
}

// An interface implemented by an object that provides data to the Tab2.
// The Tab2 sometimes owns the Tab2Model. See |removing_model_| in Tab2.
class Tab2Model {
 public:
  virtual ~Tab2Model() {}

  // Tab presentation state.
  virtual string16 GetTitle(Tab2* tab) const = 0;
  virtual SkBitmap GetIcon(Tab2* tab) const = 0;
  virtual bool IsSelected(Tab2* tab) const = 0;
  virtual bool ShouldShowIcon(Tab2* tab) const = 0;
  virtual bool IsLoading(Tab2* tab) const = 0;
  virtual bool IsCrashed(Tab2* tab) const = 0;
  virtual bool IsIncognito(Tab2* tab) const = 0;

  // The tab has been clicked and should become selected.
  virtual void SelectTab(Tab2* tab) = 0;

  // The tab should be closed.
  virtual void CloseTab(Tab2* tab) = 0;

  // The mouse has been pressed down on the tab, pertinent information for any
  // drag that might occur should be captured at this time.
  virtual void CaptureDragInfo(Tab2* tab,
                               const views::MouseEvent& drag_event) = 0;

  // The mouse has been dragged after a press on the tab.
  virtual bool DragTab(Tab2* tab, const views::MouseEvent& drag_event) = 0;

  // The current drag operation has ended.
  virtual void DragEnded(Tab2* tab) = 0;

  // TODO(beng): get rid of this once animator is on View.
  virtual views::AnimatorDelegate* AsAnimatorDelegate() = 0;
};

// A view that represents a Tab in a TabStrip2.
class Tab2 : public views::View,
             public views::ButtonListener,
             public AnimationDelegate {
 public:
  explicit Tab2(Tab2Model* model);
  virtual ~Tab2();

  bool dragging() const { return dragging_; }

  bool removing() const { return removing_; }
  void set_removing(bool removing) { removing_ = removing; }

  // Assigns and takes ownership of a model object to be used when painting this
  // Tab2 after the underlying data object has been removed from TabStrip2's
  // model.
  void SetRemovingModel(Tab2Model* model);

  // Returns true if the Tab2 is being animated.
  bool IsAnimating() const;

  // Returns the Tab2's animator, creating one if necessary.
  // TODO(beng): consider moving to views::View.
  views::Animator* GetAnimator();

  // Set the background offset used to match the image in the inactive tab
  // to the frame image.
  void set_background_offset(gfx::Point offset) {
    background_offset_ = offset;
  }

  // Set the theme provider - because we get detached, we are frequently
  // outside of a hierarchy with a theme provider at the top. This should be
  // called whenever we're detached or attached to a hierarchy.
  void set_theme_provider(ThemeProvider* provider) {
    theme_provider_ = provider;
  }

  // Adds the shape of the tab to the specified path. Used to create a clipped
  // window during detached window dragging operations.
  void AddTabShapeToPath(gfx::Path* path) const;

  // Returns the minimum possible size of a single unselected Tab.
  static gfx::Size GetMinimumUnselectedSize();

  // Returns the minimum possible size of a selected Tab. Selected tabs must
  // always show a close button and have a larger minimum size than unselected
  // tabs.
  static gfx::Size GetMinimumSelectedSize();

  // Returns the preferred size of a single Tab, assuming space is
  // available.
  static gfx::Size GetStandardSize();

  // Loads the themable resources associated with this View.
  static void LoadTabImages();

 private:
  // Possible animation states.
  enum AnimationState {
    ANIMATION_NONE,
    ANIMATION_WAITING,
    ANIMATION_LOADING
  };

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender);

  // Overridden from views::View:
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event,
                               bool canceled);
  virtual void ThemeChanged();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);
  virtual ThemeProvider* GetThemeProvider();

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationCanceled(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // Layout various portions of the tab. For each of the below, |content_height|
  // is the actual height of the content based on the font, icon size etc.
  // |content_rect| is the rectangle within which the content is laid out, and
  // may be larger.
  void LayoutIcon(int content_height, const gfx::Rect& content_rect);
  void LayoutCloseButton(int content_height, const gfx::Rect& content_rect);
  void LayoutTitle(int content_height, const gfx::Rect& content_rect);

  // Paint various portions of the tab.
  void PaintIcon(gfx::Canvas* canvas);
  void PaintTitle(gfx::Canvas* canvas);
  void PaintTabBackground(gfx::Canvas* canvas);
  void PaintInactiveTabBackground(gfx::Canvas* canvas);
  void PaintActiveTabBackground(gfx::Canvas* canvas);
  void PaintHoverTabBackground(gfx::Canvas* canvas, double opacity);
  void PaintLoadingAnimation(gfx::Canvas* canvas);

  // Returns the number of icon-size elements that can fit in the tab's
  // current size.
  int IconCapacity() const;

  // Returns whether the Tab should display a icon.
  bool ShouldShowIcon() const;

  // Returns whether the Tab should display a close button.
  bool ShouldShowCloseBox() const;

  // The object that provides state for this tab.
  Tab2Model* model_;

  // True if the Tab2 is being dragged currently.
  bool dragging_;

  // True if the Tab2 represents an object removed from its containing
  // TabStrip2's model, and is currently being animated closed.
  bool removing_;

  // Our animator.
  scoped_ptr<views::Animator> animator_;

  // A dummy model to use for painting the tab after it's been removed from the
  // TabStrip2's model but while it's still visible in the presentation (being
  // animated out of existence).
  scoped_ptr<Tab2Model> removing_model_;

  // The bounds of various sections of the display.
  gfx::Rect icon_bounds_;
  gfx::Rect title_bounds_;

  // The offset used to paint the inactive background image.
  gfx::Point background_offset_;

  // Current state of the animation.
  AnimationState animation_state_;

  // The current index into the Animation image strip.
  int animation_frame_;

  // Close Button.
  views::ImageButton* close_button_;

  // Hover animation.
  scoped_ptr<SlideAnimation> hover_animation_;

  // Pulse animation.
  scoped_ptr<ThrobAnimation> pulse_animation_;

  // Whether we're showing the icon. It is cached so that we can detect when it
  // changes and layout appropriately.
  bool showing_icon_;

  // Whether we are showing the close button. It is cached so that we can
  // detect when it changes and layout appropriately.
  bool showing_close_button_;

  // The offset used to animate the icon location.
  int icon_hiding_offset_;

  // The theme provider to source tab images from.
  ThemeProvider* theme_provider_;

  // Resources used in the tab display.
  struct TabImage {
    SkBitmap* image_l;
    SkBitmap* image_c;
    SkBitmap* image_r;
    int l_width;
    int r_width;
  };
  static TabImage tab_active_;
  static TabImage tab_inactive_;
  static TabImage tab_alpha_;

  DISALLOW_COPY_AND_ASSIGN(Tab2);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_TABS_TAB_2_H_
