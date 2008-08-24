// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/constrained_window_animation.h"
#include "chrome/browser/views/constrained_window_impl.h"

// The duration of the animation.
static const int kDuration = 360;

// The frame-rate for the animation.
static const int kFrameRate = 60;

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowAnimation, public:

ConstrainedWindowAnimation::ConstrainedWindowAnimation(
    ConstrainedWindowImpl* window)
    : Animation(kDuration, kFrameRate, NULL), window_(window) {
}

ConstrainedWindowAnimation::~ConstrainedWindowAnimation() {
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowAnimation, Animation implementation:

void ConstrainedWindowAnimation::AnimateToState(double state) {
  window_->SetTitlebarVisibilityPercentage(state);
}

