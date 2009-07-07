// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "views/animator.h"

#include <algorithm>

#include "app/slide_animation.h"
#include "views/view.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Animator, public:

Animator::Animator(View* host)
    : host_(host),
      delegate_(NULL),
      direction_(ANIMATE_NONE) {
  InitAnimation();
}

Animator::Animator(View* host, AnimatorDelegate* delegate)
    : host_(host),
      animation_(NULL),
      delegate_(delegate),
      direction_(ANIMATE_NONE) {
  InitAnimation();
}

Animator::~Animator() {
  // Explicitly NULL the delegate so we don't call back through to the delegate
  // when the animation is stopped. The Animator is designed to be owned by a
  // View and at this point the View is dust.
  delegate_ = NULL;
  animation_->Stop();
}

bool Animator::IsAnimating() const {
  return animation_->IsAnimating();
}

void Animator::AnimateToBounds(const gfx::Rect& bounds, int direction) {
  direction_ = direction;
  start_bounds_ = host_->bounds();
  target_bounds_ = bounds;

  // Stop any running animation before we have a chance to return.
  animation_->Stop();

  if (bounds == host_->bounds())
    return;

  if (direction_ == ANIMATE_NONE) {
    host_->SetBounds(bounds);
    return;
  }

  if (direction_ & ANIMATE_X) {
    if (direction_ & ANIMATE_CLAMP)
      start_bounds_.set_x(GetClampedX());
  } else {
    start_bounds_.set_x(target_bounds_.x());
  }

  if (direction_ & ANIMATE_Y) {
    if (direction_ & ANIMATE_CLAMP)
      start_bounds_.set_y(GetClampedY());
  } else {
    start_bounds_.set_y(target_bounds_.y());
  }

  if (!(direction_ & ANIMATE_WIDTH))
    start_bounds_.set_width(target_bounds_.width());
  if (!(direction_ & ANIMATE_HEIGHT))
    start_bounds_.set_height(target_bounds_.height());

  // Make sure the host view has the start bounds to avoid a flicker.
  host_->SetBounds(start_bounds_);

  // Start the animation from the beginning.
  animation_->Reset(0.0);
  animation_->Show();
}

void Animator::AnimateToBounds(int x, int y, int width, int height,
                               int direction) {
  AnimateToBounds(gfx::Rect(x, y, std::max(0, width), std::max(0, height)),
                  direction);
}

////////////////////////////////////////////////////////////////////////////////
// Animator, AnimationDelegate:

void Animator::AnimationEnded(const Animation* animation) {
  // |delegate_| could be NULL if we're called back from the destructor.
  if (delegate_)
    delegate_->AnimationCompletedForHost(host_);
}

void Animator::AnimationProgressed(const Animation* animation) {
  int delta_x = target_bounds_.x() - start_bounds_.x();
  int delta_y = target_bounds_.y() - start_bounds_.y();
  int delta_width =  target_bounds_.width() - start_bounds_.width();
  int delta_height = target_bounds_.height() - start_bounds_.height();

  // The current frame's position and size is the animation's progress percent
  // multiplied by the delta between the start and target position/size...
  double cv = animation_->GetCurrentValue();
  int frame_x = start_bounds_.x() + static_cast<int>(delta_x * cv);
  int frame_y = start_bounds_.y() + static_cast<int>(delta_y * cv);
  // ... except for clamped positions, which remain clamped at the right/bottom
  // edge of the previous view in the layout flow.
  if (direction_ & ANIMATE_CLAMP && direction_ & ANIMATE_X)
    frame_x = GetClampedX();
  if (direction_ & ANIMATE_CLAMP && direction_ & ANIMATE_Y)
    frame_y = GetClampedY();
  int frame_width = start_bounds_.width() + static_cast<int>(delta_width * cv);
  int frame_height =
      start_bounds_.height() + static_cast<int>(delta_height * cv);
  host_->SetBounds(frame_x, frame_y, frame_width, frame_height);
  host_->GetParent()->SchedulePaint();
}

void Animator::AnimationCanceled(const Animation* animation) {
  AnimationEnded(animation);
}

////////////////////////////////////////////////////////////////////////////////
// Animator, private:

void Animator::InitAnimation() {
  animation_.reset(new SlideAnimation(this));
  animation_->SetSlideDuration(150);
  animation_->SetTweenType(SlideAnimation::EASE_OUT);
}

int Animator::GetClampedX() const {
  if (delegate_ && direction_ & ANIMATE_CLAMP && direction_ & ANIMATE_X) {
    View* previous_view = delegate_->GetClampedView(host_);
    if (previous_view)
      return previous_view->bounds().right();
  }
  return host_->x();
}

int Animator::GetClampedY() const {
  if (delegate_ && direction_ & ANIMATE_CLAMP && direction_ & ANIMATE_Y) {
    View* previous_view = delegate_->GetClampedView(host_);
    if (previous_view)
      return previous_view->bounds().bottom();
  }
  return host_->y();
}

}  // namespace views
