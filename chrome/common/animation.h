// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Inspired by NSAnimation

#ifndef CHROME_COMMON_ANIMATION_H__
#define CHROME_COMMON_ANIMATION_H__

#include "base/timer.h"

class Animation;

// AnimationDelegate
//
//  Implement this interface when you want to receive notifications about the
//  state of an animation.
//
class AnimationDelegate {
 public:
  // Called when an animation has started.
  virtual void AnimationStarted(const Animation* animation) {
  }

  // Called when an animation has completed.
  virtual void AnimationEnded(const Animation* animation) {
  }

  // Called when an animation has progressed.
  virtual void AnimationProgressed(const Animation* animation) {
  }

  // Called when an animation has been canceled.
  virtual void AnimationCanceled(const Animation* animation) {
  }
};

// Animation
//
//  This class provides a basic implementation of an object that uses a timer
//  to increment its state over the specified time and frame-rate. To
//  actually do something useful with this you need to subclass it and override
//  AnimateToState and optionally GetCurrentValue to update your state.
//
//  The Animation notifies a delegate when events of interest occur.
//
//  The practice is to instantiate a subclass and call Init and any other
//  initialization specific to the subclass, and then call |Start|. The
//  animation uses the current thread's message loop.
//
class Animation {
 public:
  // Initializes everything except the duration.
  //
  // Caller must make sure to call SetDuration() if they use this
  // constructor; it is preferable to use the full one, but sometimes
  // duration can change between calls to Start() and we need to
  // expose this interface.
  Animation(int frame_rate, AnimationDelegate* delegate);

  // Initializes all fields.
  Animation(int duration, int frame_rate, AnimationDelegate* delegate);
  virtual ~Animation();

  // Reset state so that the animation can be started again.
  virtual void Reset();

  // Called when the animation progresses. Subclasses override this to
  // efficiently update their state.
  virtual void AnimateToState(double state) = 0;

  // Gets the value for the current state, according to the animation
  // curve in use. This class provides only for a linear relationship,
  // however subclasses can override this to provide others.
  virtual double GetCurrentValue() const;

  // Start the animation.
  void Start();

  // Stop the animation.
  void Stop();

  // Skip to the end of the current animation.
  void End();

  // Return whether this animation is animating.
  bool IsAnimating();

  // Changes the length of the animation. This resets the current
  // state of the animation to the beginning.
  void SetDuration(int duration);

 protected:
  // Overriddable, called by Run.
  virtual void Step();

  // Calculates the timer interval from the constructor list.
  int CalculateInterval(int frame_rate);

  // Whether or not we are currently animating.
  bool animating_;

  int frame_rate_;
  int timer_interval_;
  int duration_;

  // For determining state.
  int iteration_count_;
  int current_iteration_;
  double state_;

  AnimationDelegate* delegate_;

  base::RepeatingTimer<Animation> timer_;

 private:
  // Called when the animation's timer expires, calls Step.
  void Run();

  DISALLOW_EVIL_CONSTRUCTORS(Animation);
};

#endif  // CHROME_COMMON_ANIMATION_H__
