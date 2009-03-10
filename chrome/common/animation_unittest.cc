// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/common/animation.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace std;

class AnimationTest: public testing::Test {
 private:
  MessageLoopForUI message_loop_;
};

///////////////////////////////////////////////////////////////////////////////
// RunAnimation

class RunAnimation : public Animation {
 public:
  RunAnimation(int frame_rate, AnimationDelegate* delegate)
      : Animation(frame_rate, delegate) {
  }

  virtual void AnimateToState(double state) {
    EXPECT_LE(0.0, state);
    EXPECT_GE(1.0, state);
  }
};

///////////////////////////////////////////////////////////////////////////////
// CancelAnimation

class CancelAnimation : public Animation {
 public:
  CancelAnimation(int duration, int frame_rate, AnimationDelegate* delegate)
      : Animation(duration, frame_rate, delegate) {
  }

  virtual void AnimateToState(double state) {
    if (state >= 0.5)
      Stop();
  }
};

///////////////////////////////////////////////////////////////////////////////
// LinearCase

class TestAnimationDelegate : public AnimationDelegate {
 public:
  TestAnimationDelegate() :
    canceled_(false),
    finished_(false) {
  }

  virtual void AnimationStarted(const Animation* animation) {
  }

  virtual void AnimationEnded(const Animation* animation) {
    finished_ = true;
    MessageLoop::current()->Quit();
  }

  virtual void AnimationCanceled(const Animation* animation) {
    finished_ = true;
    canceled_ = true;
    MessageLoop::current()->Quit();
  }

  bool finished() {
    return finished_;
  }

  bool canceled() {
    return canceled_;
  }

 private:
  bool canceled_;
  bool finished_;
};

TEST_F(AnimationTest, RunCase) {
  TestAnimationDelegate ad;
  RunAnimation a1(150, &ad);
  a1.SetDuration(2000);
  a1.Start();
  MessageLoop::current()->Run();

  EXPECT_TRUE(ad.finished());
  EXPECT_FALSE(ad.canceled());
}

TEST_F(AnimationTest, CancelCase) {
  TestAnimationDelegate ad;
  CancelAnimation a2(2000, 150, &ad);
  a2.Start();
  MessageLoop::current()->Run();

  EXPECT_TRUE(ad.finished());
  EXPECT_TRUE(ad.canceled());
}
