// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SLIDE_ANIMATION_H_
#define CHROME_COMMON_SLIDE_ANIMATION_H_

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
  bool IsShowing() const { return showing_; }

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

#endif  // CHROME_COMMON_SLIDE_ANIMATION_H_
