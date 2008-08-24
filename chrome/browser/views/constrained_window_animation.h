// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_CONSTRAINED_WINDOW_ANIMATION_H__
#define CHROME_BROWSER_VIEWS_CONSTRAINED_WINDOW_ANIMATION_H__

#include "chrome/common/animation.h"

class ConstrainedWindowImpl;

// Animates a titlebar of a suppressed constrained window up from the
// bottom of the screen.
class ConstrainedWindowAnimation : public Animation {
 public:
  explicit ConstrainedWindowAnimation(ConstrainedWindowImpl* window);
  virtual ~ConstrainedWindowAnimation();

  // Overridden from Animation:
  virtual void AnimateToState(double state);

 private:
  // The constrained window we're displaying.
  ConstrainedWindowImpl* window_;

  DISALLOW_EVIL_CONSTRUCTORS(ConstrainedWindowAnimation);
};

#endif  // CHROME_BROWSER_VIEWS_CONSTRAINED_WINDOW_ANIMATION_H__

