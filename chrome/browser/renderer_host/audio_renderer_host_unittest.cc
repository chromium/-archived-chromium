// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/renderer_host/audio_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

class AudioRendererHostTest : public testing::Test {
 protected:
  virtual void SetUp() {
    host_.reset(new AudioRendererHost(MessageLoop::current()));
  }

  scoped_ptr<AudioRendererHost> host_;
};

TEST_F(AudioRendererHostTest, NoTest) {
  // TODO(hclam): come up with useful tests.
}
