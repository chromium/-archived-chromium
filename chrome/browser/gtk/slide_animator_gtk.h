// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper class for animating the display of native widget content.
// Currently only handle vertical sliding, but could be extended to handle
// horizontal slides or other types of animations.
//
// NOTE: This does not handle clipping. If you are not careful, you will
// wind up with visibly overlapping widgets. If you need clipping, you can
// extend the constructor to take an option to give |fixed| its own GdkWindow
// (via gtk_fixed_set_has_window).

#ifndef CHROME_BROWSER_GTK_SLIDE_ANIMATOR_GTK_H_
#define CHROME_BROWSER_GTK_SLIDE_ANIMATOR_GTK_H_

#include <gtk/gtk.h>

#include "app/animation.h"
#include "base/scoped_ptr.h"
#include "chrome/common/owned_widget_gtk.h"

class SlideAnimation;

class SlideAnimatorGtk : public AnimationDelegate {
 public:
  class Delegate {
   public:
    // Called when a call to Close() finishes animating.
    virtual void Closed() = 0;
  };

  enum Direction {
    DOWN,
    UP
  };

  // |child| is the widget we pack into |widget_|.
  // |direction| indicates which side the contents will appear to come from.
  // |duration| is the duration of the slide in milliseconds, or 0 for default.
  // |linear| controls how the animation progresses. If true, the
  // velocity of the slide is constant over time, otherwise it goes a bit faster
  // at the beginning and slows to a halt.
  // |delegate| may be NULL.
  SlideAnimatorGtk(GtkWidget* child,
                   Direction direction,
                   int duration,
                   bool linear,
                   Delegate* delegate);

  virtual ~SlideAnimatorGtk();

  GtkWidget* widget() { return widget_.get(); }

  // Slide open.
  void Open();

  // Immediately show the widget.
  void OpenWithoutAnimation();

  // Slide shut.
  void Close();

  // Immediately hide the widget.
  void CloseWithoutAnimation();

  // Returns whether the widget is visible.
  bool IsShowing();

  // Returns whether the widget is currently showing the close animation.
  bool IsClosing();

  // AnimationDelegate implementation.
  void AnimationProgressed(const Animation* animation);
  void AnimationEnded(const Animation* animation);

 private:
  static void OnChildSizeAllocate(GtkWidget* child,
                                  GtkAllocation* allocation,
                                  SlideAnimatorGtk* slider);

  scoped_ptr<SlideAnimation> animation_;

  // The top level widget of the SlideAnimatorGtk. It is a GtkFixed.
  OwnedWidgetGtk widget_;

  // The widget passed to us at construction time, and the only direct child of
  // |widget_|.
  GtkWidget* child_;

  // The direction of the slide.
  Direction direction_;

  // The object to inform about certain events. It may be NULL.
  Delegate* delegate_;

  // If true, we should resize |widget_| on the next "size-allocate" event that
  // is received by |child_|. See the comment in SlideAnimatorGtk constructor.
  bool fixed_needs_resize_;

  // We need to move the child widget to (0, -height), but we don't know its
  // height until it has been allocated. This variable will be true until the
  // child widget has been allocated, at which point we will move it, and then
  // set this variable to false to indicate it should not be moved again.
  bool child_needs_move_;
};

#endif  // CHROME_BROWSER_GTK_SLIDE_ANIMATOR_GTK_H_
