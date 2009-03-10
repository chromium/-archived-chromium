// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_THROB_ANIMATION_H_
#define CHROME_COMMON_THROB_ANIMATION_H_

#include "chrome/common/slide_animation.h"

// A subclass of SlideAnimation that can continually slide. All of the Animation
// methods behave like that of SlideAnimation: transition to the next state.
// The StartThrobbing method causes the ThrobAnimation to cycle between hidden
// and shown for a set number of cycles.
//
// A ThrobAnimation has two durations: the duration used when behavior like
// a SlideAnimation, and the duration used when throbbing.
class ThrobAnimation : public SlideAnimation {
 public:
  explicit ThrobAnimation(AnimationDelegate* target);
  virtual ~ThrobAnimation() {}

  // Starts throbbing. cycles_til_stop gives the number of cycles to do before
  // stopping.
  void StartThrobbing(int cycles_til_stop);

  // Sets the duration of the slide animation when throbbing.
  void SetThrobDuration(int duration) { throb_duration_ = duration; }

  // Overridden to reset to the slide duration.
  virtual void Reset();
  virtual void Show();
  virtual void Hide();

  // Overriden to continually throb (assuming we're throbbing).
  virtual void Step();

  // Overridden to maintain the slide duration.
  virtual void SetSlideDuration(int duration) { slide_duration_ = duration; }

 private:
  // Resets state such that we behave like SlideAnimation.
  void ResetForSlide();

  // Duration of the slide animation.
  int slide_duration_;

  // Duration of the slide animation when throbbing.
  int throb_duration_;

  // If throbbing, this is the number of cycles left.
  int cycles_remaining_;

  // Are we throbbing?
  bool throbbing_;

  DISALLOW_EVIL_CONSTRUCTORS(ThrobAnimation);
};

#endif  // CHROME_COMMON_THROB_ANIMATION_H_
