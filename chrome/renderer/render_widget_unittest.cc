// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/ref_counted.h"
#include "chrome/renderer/mock_render_process.h"
#include "chrome/renderer/mock_render_thread.h"
#include "chrome/renderer/render_widget.h"
#include "chrome/renderer/render_thread.h"

namespace {

const int32 kRouteId = 5;
const int32 kOpenerId = 7;

class RenderWidgetTest : public testing::Test {
 public:

 protected:
  MessageLoop msg_loop_;
  MockRenderThread render_thread_;

  // The widget, each test should verify this is non-NULL before continuing.
  scoped_refptr<RenderWidget> widget_;

 private:
  // testing::Test
  virtual void SetUp() {
    mock_process_.reset(new MockProcess());
    render_thread_.set_routing_id(kRouteId);
    widget_ = RenderWidget::Create(kOpenerId, &render_thread_, true);
    ASSERT_TRUE(widget_);
  }
  virtual void TearDown() {
    widget_ = NULL;
    mock_process_.reset();
  }

  scoped_ptr<MockProcess> mock_process_;
};

TEST_F(RenderWidgetTest, CreateAndCloseWidget) {
  // After the RenderWidget it must have sent a message to the render thread
  // that sets the opener id.
  EXPECT_EQ(kOpenerId, render_thread_.opener_id());
  ASSERT_TRUE(render_thread_.has_widget());

  // Now simulate a close of the Widget.
  render_thread_.SendCloseMessage();
  EXPECT_FALSE(render_thread_.has_widget());

  // Run the loop so the release task from the renderwidget executes.
  msg_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask());
  msg_loop_.Run();
}

}  // namespace
