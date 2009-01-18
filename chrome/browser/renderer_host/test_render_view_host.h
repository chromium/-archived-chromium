// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_TEST_RENDER_VIEW_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_TEST_RENDER_VIEW_HOST_H_

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/renderer_host/mock_render_process_host.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file provides a testing framework for mocking out the RenderProcessHost
// layer. It allows you to test RenderViewHost, WebContents,
// NavigationController, and other layers above that without running an actual
// renderer process.
//
// To use, derive your test base class from RenderViewHostTestHarness.

class TestRenderViewHost : public RenderViewHost {
 public:
  TestRenderViewHost(SiteInstance* instance,
                     RenderViewHostDelegate* delegate,
                     int routing_id,
                     base::WaitableEvent* modal_dialog_event)
      : RenderViewHost(instance, delegate, routing_id, modal_dialog_event) {
  }

  // Calls OnMsgNavigate on the RenderViewHost with the given information,
  // setting the rest of the parameters in the message to the "typical" values.
  // This is a helper function for simulating the most common types of loads.
  void SendNavigate(int page_id, const GURL& url);

 private:
  FRIEND_TEST(RenderViewHostTest, FilterNavigate);

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewHost);
};

class TestRenderViewHostFactory : public RenderViewHostFactory {
 public:
  TestRenderViewHostFactory() {}
  virtual ~TestRenderViewHostFactory() {}

  virtual RenderViewHost* CreateRenderViewHost(
      SiteInstance* instance,
      RenderViewHostDelegate* delegate,
      int routing_id,
      base::WaitableEvent* modal_dialog_event) {
    return new TestRenderViewHost(instance, delegate, routing_id,
                                  modal_dialog_event);
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewHostFactory);
};

class RenderViewHostTestHarness : public testing::Test {
 public:
  RenderViewHostTestHarness()
      : process_(NULL),
        contents_(NULL),
        controller_(NULL) {}
  ~RenderViewHostTestHarness() {}

  TestRenderViewHost* rvh() {
    return reinterpret_cast<TestRenderViewHost*>(contents_->render_view_host());
  }

 protected:
  // testing::Test
  virtual void SetUp();
  virtual void TearDown();

  MessageLoopForUI message_loop_;
  TestingProfile profile_;

  TestRenderViewHostFactory rvh_factory_;

  // We clean up the WebContents by calling CloseContents, which deletes itself.
  // This in turn causes the destruction of these other things.
  MockRenderProcessHost* process_;
  WebContents* contents_;
  NavigationController* controller_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestHarness);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_TEST_RENDER_VIEW_HOST_H_
