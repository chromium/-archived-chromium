// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFO_BAR_ITEM_VIEW_H__
#define CHROME_BROWSER_VIEWS_INFO_BAR_ITEM_VIEW_H__

#include "chrome/browser/views/info_bar_view.h"
#include "chrome/common/slide_animation.h"
#include "chrome/views/button.h"

namespace views {
class ExternalFocusTracker;
class ImageView;
}
// Note: An InfoBarItemView must be added as a child of InfoBarView to be
// displayed correctly.
//
// InfoBarItemView is basically a view container that lays out views in a
// horizontal row, on either the left or the right, with specified padding. It
// has a close button on the far right, which can't be removed, and closes the
// info bar by default. An icon can be set to be displayed leading all other
// views by calling SetIcon().
//
// A view can be added to either the left or the right of the info bar by
// calling AddChildViewTrailing and AddChildViewLeading.
//
// The most recently added views to either side will always be located further
// towards the center than views added less recently, with the first views added
// to the left or right being located on the leftmost or rightmost sides of the
// info bar, respectively. Each view has a default spacing from the next
// view added to that side, but you can edit that by specifying a padding when
// you add a view. For example, if you add a view to the left with a padding of
// 6 specified, it will be placed in the leftmost position, and the next view
// added to the left will be 6 pixels to the right of the previously added view.
class InfoBarItemView : public views::View,
                        public views::BaseButton::ButtonListener,
                        public AnimationDelegate {
 public:
  InfoBarItemView();

  virtual ~InfoBarItemView();

  // The preferred height is equal to the maximum height of all views
  // in the info bar. Preferred width is equal to the parents width.
  virtual gfx::Size GetPreferredSize();

  // Lays out all child views of the info bar from trailing to leading.
  virtual void Layout();

  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);

  // Starts the close animation, which will end in the bar closing itself.
  void BeginClose();

  // Removes this InfoBarItem from its parent view and then deletes it.
  void Close();

  // ButtonListener Method
  // Calls CloseButtonPressed() when the close button is pressed
  virtual void ButtonPressed(views::BaseButton* button);

  // Adds |view| to the info bar, directly leading the last trailing view
  // added, according to that views specified padding. The next trailing view
  // added will in turn be leading this view by |leading_padding| pixels.
  // Specify 0 for |leading_padding| if the views should be flush.
  void AddChildViewTrailing(views::View* view, int leading_padding);

  // Calls AddChildViewTrailing with a default amount of padding.
  void AddChildViewTrailing(views::View* view);

  // Adds |view| to the info bar, directly trailing the last leading view
  // added, according to that views specified padding. The next leading view
  // added will in turn be trailing this view by |trailing_padding| pixels.
  // Specify 0 for |trailing_padding| if the views should be flush.
  void AddChildViewLeading(views::View* view, int trailing_padding);

  // Calls AddChildViewLeading with a default amount of padding.
  void AddChildViewLeading(views::View* view);

  // Sets the icon to be displayed leading all other views in the info bar.
  // The icon will be displayed at its images height and width by default.
  void SetIcon(const SkBitmap& icon);

 protected:
  // Returns the desired position for a centered object of size |size| within
  // a region of size |target_size|.
  static int CenterPosition(int size, int target_size);

  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

  // Overridden from the basic Views AddChildView. Calls
  // AddChildViewTrailing(view)
  virtual void AddChildView(views::View* view);

  // Overridden from basic View. Adds the view to the same side as the view
  // at index. Does *not* insert at the specified index, or even neccesarily
  // close to it.
  virtual void AddChildView(int index, views::View* view);

  // Overridden from the basic Views AddChildView, removes the specified view
  // as well as its padding.
  virtual void RemoveChildView(views::View* view);

  // Invoked whenever the close button is pressed. Closes infobar by default.
  virtual void CloseButtonPressed();

 private:
  // Creates cancel button.
  void Init();

  // SlideAnimationDelegate implementation.
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  scoped_ptr<SlideAnimation> animation_;

  // View index where all new views will be inserted. Any view at an index less
  // than insert_index will be laid out will be leading views (left aligned in
  // left to right languages), any view greater than or equal to insert_index_
  // will be laid out trailing (right aligned in left to right languages).
  int insert_index_;

  // Dismisses the info bar by default.
  views::Button* close_button_;

  // Optional icon to be displayed at the far left of the infobar.
  views::ImageView* icon_;

  // Tracks and stores the last focused view which is not the InfoBarItemView or
  // any of its children. Used to restore focus once the InfoBarItemView is
  // closed.
  scoped_ptr<views::ExternalFocusTracker> focus_tracker_;

  DISALLOW_EVIL_CONSTRUCTORS(InfoBarItemView);
};

#endif // CHROME_BROWSER_VIEWS_INFO_BAR_ITEM_VIEW_H__

