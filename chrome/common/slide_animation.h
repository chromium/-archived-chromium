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

#ifndef CHROME_SLIDE_COMMON_ANIMATION_H__
#define CHROME_SLIDE_COMMON_ANIMATION_H__

#include "chrome/common/animation.h"

// Slide Animation
//
// Used for reversible animations and as a general helper class. Typical usage:
//
// #include "chrome/common/slide_animation.h"
//
// class MyClass : public AnimationDelegate {
//  public:
//   MyClass() {
//     animation_.reset(new SlideAnimation(this));
//     animation_->SetSlideDuration(500);
//   }
//   void OnMouseOver() {
//     animation_->Show();
//   }
//   void OnMouseOut() {
//     animation_->Hide();
//   }
//   void AnimationProgressed(const Animation* animation) {
//     if (animation == animation_.get()) {
//       Layout();
//       SchedulePaint();
//     } else if (animation == other_animation_.get()) {
//       ...
//     }
//   }
//   void Layout() {
//     if (animation_->IsAnimating()) {
//       hover_image_.SetOpacity(animation_->GetCurrentValue());
//     }
//   }
//  private:
//   scoped_ptr<SlideAnimation> animation_;
// }
class SlideAnimation : public Animation {
 public:
  explicit SlideAnimation(AnimationDelegate* target);
  virtual ~SlideAnimation();

  enum TweenType {
    NONE,          // Default linear.
    EASE_OUT,      // Fast in, slow out.
    EASE_IN,       // Slow in, fast out.
    EASE_IN_OUT,   // Slow in and out, fast in the middle.
    FAST_IN_OUT,   // Fast in and out, slow in the middle.
    EASE_OUT_SNAP, // Fast in, slow out, snap to final value.
  };

  // Set the animation back to the 0 state.
  virtual void Reset();
  virtual void Reset(double value);

  // Begin a showing animation or reverse a hiding animation in progress.
  virtual void Show();

  // Begin a hiding animation or reverse a showing animation in progress.
  virtual void Hide();

  // Sets the time a slide will take. Note that this isn't actually
  // the amount of time an animation will take as the current value of
  // the slide is considered.
  virtual void SetSlideDuration(int duration) { slide_duration_ = duration; }
  int GetSlideDuration() const { return slide_duration_; }
  void SetTweenType(TweenType tween_type) { tween_type_ = tween_type; }

  double GetCurrentValue() const { return value_current_; }
  bool IsShowing() { return showing_; }

 private:
  // Overridden from Animation.
  void AnimateToState(double state);

  AnimationDelegate* target_;

  TweenType tween_type_;

  // Used to determine which way the animation is going.
  bool showing_;

  // Animation values. These are a layer on top of Animation::state_ to
  // provide the reversability.
  double value_start_;
  double value_end_;
  double value_current_;

  // How long a hover in/out animation will last for. This defaults to
  // kHoverFadeDurationMS, but can be overridden with SetDuration.
  int slide_duration_;
};

#endif  // CHROME_COMMON_SLIDE_ANIMATION_H__
