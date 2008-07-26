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
// Inspired by NSAnimation

#ifndef CHROME_COMMON_ANIMATION_H__
#define CHROME_COMMON_ANIMATION_H__

#include "base/task.h"
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
class Animation : public Task {
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

  // The animation's Task::Run implementation
  virtual void Run();

  // Changes the length of the animation. This resets the current
  // state of the animation to the beginning.
  void SetDuration(int duration);

 protected:
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

  RepeatingTimer timer_;

  DISALLOW_EVIL_CONSTRUCTORS(Animation);
};

#endif  // CHROME_COMMON_ANIMATION_H__
