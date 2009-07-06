// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/navigation_entry.h"

class RenderViewHostTest : public RenderViewHostTestHarness {
};

// All about URLs reported by the renderer should get rewritten to about:blank.
// See RenderViewHost::OnMsgNavigate for a discussion.
TEST_F(RenderViewHostTest, FilterAbout) {
  rvh()->SendNavigate(1, GURL("about:cache"));
  ASSERT_TRUE(controller().GetActiveEntry());
  EXPECT_EQ(GURL("about:blank"), controller().GetActiveEntry()->url());
}
