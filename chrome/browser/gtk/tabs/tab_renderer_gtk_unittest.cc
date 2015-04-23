// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tabs/tab_renderer_gtk.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef TabRendererGtk::LoadingAnimation LoadingAnimation;

const int kTestLoadingAnimationFrameCount = 10;
const int kTestWaitingAnimationFrameCount = 40;
const int kTestWaitingToLoadingFrameCountRatio = 4;

const LoadingAnimation::Data kMockAnimationData = {
  NULL,  // waiting_animation_frames
  NULL,  // loading_animation_frames
  kTestLoadingAnimationFrameCount,
  kTestWaitingAnimationFrameCount,
  kTestWaitingToLoadingFrameCountRatio,
};

TEST(LoadingAnimationTest, InitialState) {
  LoadingAnimation loading_animation(&kMockAnimationData);
  EXPECT_EQ(TabRendererGtk::ANIMATION_NONE,
            loading_animation.animation_state());
  EXPECT_EQ(0, loading_animation.animation_frame());
}

TEST(LoadingAnimationTest, AdvanceWaitingFrames) {
  LoadingAnimation loading_animation(&kMockAnimationData);
  loading_animation.ValidateLoadingAnimation(TabRendererGtk::ANIMATION_WAITING);
  EXPECT_EQ(TabRendererGtk::ANIMATION_WAITING,
            loading_animation.animation_state());
  EXPECT_EQ(1, loading_animation.animation_frame());
  loading_animation.ValidateLoadingAnimation(TabRendererGtk::ANIMATION_WAITING);
  EXPECT_EQ(TabRendererGtk::ANIMATION_WAITING,
            loading_animation.animation_state());
  EXPECT_EQ(2, loading_animation.animation_frame());

  for (int i = 2; i < kTestWaitingAnimationFrameCount - 1; ++i) {
    loading_animation.ValidateLoadingAnimation(
        TabRendererGtk::ANIMATION_WAITING);
  }
  EXPECT_EQ(TabRendererGtk::ANIMATION_WAITING,
            loading_animation.animation_state());
  EXPECT_EQ(kTestWaitingAnimationFrameCount - 1,
            loading_animation.animation_frame());

  loading_animation.ValidateLoadingAnimation(TabRendererGtk::ANIMATION_WAITING);
  EXPECT_EQ(TabRendererGtk::ANIMATION_WAITING,
            loading_animation.animation_state());
  EXPECT_EQ(0, loading_animation.animation_frame()) <<
            "The animation frame should have wrapped around to 0.";
}

TEST(LoadingAnimationTest, AdvanceFromWaitingToLoading) {
  LoadingAnimation loading_animation(&kMockAnimationData);
  for (int i = 0; i < 2 * kTestWaitingToLoadingFrameCountRatio; ++i) {
    loading_animation.ValidateLoadingAnimation(
        TabRendererGtk::ANIMATION_WAITING);
  }
  EXPECT_EQ(TabRendererGtk::ANIMATION_WAITING,
            loading_animation.animation_state());
  EXPECT_EQ(2 * kTestWaitingToLoadingFrameCountRatio,
            loading_animation.animation_frame());

  loading_animation.ValidateLoadingAnimation(TabRendererGtk::ANIMATION_LOADING);
  EXPECT_EQ(TabRendererGtk::ANIMATION_LOADING,
            loading_animation.animation_state());
  EXPECT_EQ(kTestLoadingAnimationFrameCount - 1,
            loading_animation.animation_frame());
}

}  // namespace
