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
    // Create a message loop so AudioRendererHost can use it.
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));
    host_ = new AudioRendererHost(MessageLoop::current());
  }

  virtual void TearDown() {
    host_->Destroy();
  }

  scoped_refptr<AudioRendererHost> host_;
  scoped_ptr<MessageLoop> message_loop_;
};

TEST_F(AudioRendererHostTest, NoTest) {
  // TODO(hclam): come up with useful tests.
}
