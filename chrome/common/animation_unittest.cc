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

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/common/animation.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace std;

namespace {
  class AnimationTest: public testing::Test {
  };
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
  bool finished_;
  bool canceled_;
};

TEST(AnimationTest, RunCase) {
  TestAnimationDelegate ad;
  RunAnimation a1(150, &ad);
  a1.SetDuration(2000);
  a1.Start();
  MessageLoop::current()->Run();

  EXPECT_TRUE(ad.finished());
  EXPECT_FALSE(ad.canceled());
}

TEST(AnimationTest, CancelCase) {
  TestAnimationDelegate ad;
  CancelAnimation a2(2000, 150, &ad);
  a2.Start();
  MessageLoop::current()->Run();

  EXPECT_TRUE(ad.finished());
  EXPECT_TRUE(ad.canceled());
}
