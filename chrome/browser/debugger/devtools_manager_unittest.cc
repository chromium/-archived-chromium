// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/renderer_host/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/common/render_messages.h"

namespace {

class TestDevToolsWindow : public DevToolsWindow {
 public:
  TestDevToolsWindow(DevToolsInstanceDescriptor* descriptor)
      : descriptor_(descriptor),
        shown_(false),
        closed_(false) {
    descriptor->SetDevToolsWindow(this);
  }

  virtual ~TestDevToolsWindow() {
    EXPECT_EQ(shown_, closed_);
  }

  virtual void Show() {
    open_counter++;
    shown_ = true;
  }

  virtual void Close() {
    EXPECT_TRUE(shown_);
    close_counter++;
    descriptor_->Destroy();
    closed_ = true;
  }

  static void ResetCounters() {
    open_counter = 0;
    close_counter = 0;
  }

  static int open_counter;
  static int close_counter;

 private:
  DevToolsInstanceDescriptor* descriptor_;
  bool shown_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(TestDevToolsWindow);
};

int TestDevToolsWindow::open_counter = 0;
int TestDevToolsWindow::close_counter = 0;


class TestDevToolsWindowFactory : public DevToolsWindowFactory {
 public:
  TestDevToolsWindowFactory() : DevToolsWindowFactory(),
                                last_created_window(NULL) {}
  virtual ~TestDevToolsWindowFactory() {}

  virtual DevToolsWindow* CreateDevToolsWindow(
      DevToolsInstanceDescriptor* descriptor) {
    last_created_window = new TestDevToolsWindow(descriptor);
    return last_created_window;
  }

  DevToolsWindow* last_created_window;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDevToolsWindowFactory);
};

}  // namespace

class DevToolsManagerTest : public RenderViewHostTestHarness {
 public:
  DevToolsManagerTest() : RenderViewHostTestHarness() {
  }

 protected:
  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    TestDevToolsWindow::ResetCounters();
  }
};

TEST_F(DevToolsManagerTest, OpenAndManuallyCloseDevToolsWindow) {
  TestDevToolsWindowFactory window_factory;
  DevToolsManager manager(&window_factory);

  manager.ShowDevToolsForWebContents(contents());
  EXPECT_EQ(TestDevToolsWindow::open_counter, 1);
  EXPECT_EQ(TestDevToolsWindow::close_counter, 0);

  DevToolsWindow* window = window_factory.last_created_window;
  // Test that same devtools window is used.
  manager.ShowDevToolsForWebContents(contents());
  // Check that no new windows were created.
  EXPECT_TRUE(window == window_factory.last_created_window);
  EXPECT_EQ(TestDevToolsWindow::open_counter, 2);
  EXPECT_EQ(TestDevToolsWindow::close_counter, 0);

  window->Close();
  EXPECT_EQ(TestDevToolsWindow::open_counter, 2);
  EXPECT_EQ(TestDevToolsWindow::close_counter, 1);
}
