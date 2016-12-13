// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_ANIMATOR_H_
#define VIEWS_ANIMATOR_H_

#include "app/animation.h"
#include "base/gfx/rect.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

class SlideAnimation;

////////////////////////////////////////////////////////////////////////////////
// ALERT!
//
// This API is preliminary and subject to change. Talk to beng before using it!
//

namespace views {

class View;

class AnimatorDelegate {
 public:
  // Returns the view in the visual layout whose trailing edge the view that
  // hosts an animator should be clamped to during animations, when
  // ANIMATE_CLAMP is specified in combination with ANIMATE_X or ANIMATE_Y.
  virtual View* GetClampedView(View* host) = 0;

  // Notifies the delegate that the active animation running for |host| has
  // completed.
  virtual void AnimationCompletedForHost(View* host) = 0;
};

// An animator is an object that can animate actions on a host view. Once
// created, an Animator is typically owned by its host view.
class Animator : public AnimationDelegate {
 public:
  enum BoundsChangeFlags {
    ANIMATE_NONE = 0x0,     // Don't animate anything... o_O
    ANIMATE_X = 0x1,        // Animate the host view's x position
    ANIMATE_Y = 0x2,        // Animate the host view's y position
    ANIMATE_WIDTH = 0x4,    // Animate the host view's width
    ANIMATE_HEIGHT = 0x8,   // Animate the host view's height
    ANIMATE_CLAMP = 0x10    // Clamp the host view's x or y position to the
                            // trailing edge of the view returned by
                            // AnimatorDelegate::GetClampedView.
  };

  // Creates the animator for the specified host view. Optionally an
  // AnimationContext can be provided to animate multiple views from a single
  // animation.
  explicit Animator(View* host);
  Animator(View* host, AnimatorDelegate* delegate);
  virtual ~Animator();

  // Returns true if the animator is currently animating.
  bool IsAnimating() const;

  // Moves/sizes the host view to the specified bounds. |direction| is a
  // combination of the above flags indicating what aspects of the bounds should
  // be animated.
  void AnimateToBounds(const gfx::Rect& bounds, int direction);
  void AnimateToBounds(int x, int y, int width, int height, int direction);

  // Overridden from AnimationDelegate:
  virtual void AnimationEnded(const Animation* animation);
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationCanceled(const Animation* animation);

 private:
  void InitAnimation();

  // Get the current X/Y position of the host view, clamped to the right edge of
  // the previous view in the visual layout, if applicable (See
  // AnimatorDelegate for more info).
  int GetClampedX() const;
  int GetClampedY() const;

  // The view that this animator is attached to.
  View* host_;

  // Start and end bounds for the animation.
  gfx::Rect start_bounds_;
  gfx::Rect target_bounds_;

  // The animation used by this animator.
  scoped_ptr<SlideAnimation> animation_;

  // A delegate object that provides information about surrounding views.
  // Will be NULL during this class' destructor.
  AnimatorDelegate* delegate_;

  // Some combination of BoundsChangeFlags indicating the type of bounds change
  // the host view is subject to.
  int direction_;

  DISALLOW_COPY_AND_ASSIGN(Animator);
};

}  // namespace views

#endif  // #ifndef VIEWS_ANIMATOR_H_
