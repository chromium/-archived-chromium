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
  virtual void Run();

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
