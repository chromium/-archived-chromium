// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper class for animating the display of native widget content.
// Currently only handle vertical sliding, but could be extended to handle
// horizontal slides or other types of animations.

#ifndef CHROME_BROWSER_GTK_SLIDE_ANIMATOR_GTK_H_
#define CHROME_BROWSER_GTK_SLIDE_ANIMATOR_GTK_H_

#include "base/scoped_ptr.h"
#include "chrome/common/animation.h"
#include "chrome/common/owned_widget_gtk.h"

class SlideAnimation;

typedef struct _GtkWidget GtkWidget;

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

  // Returns whether the widget is visible.
  bool IsShowing();

  // AnimationDelegate implementation.
  void AnimationProgressed(const Animation* animation);
  void AnimationEnded(const Animation* animation);

 private:
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
};

#endif  // CHROME_BROWSER_GTK_SLIDE_ANIMATOR_GTK_H_
