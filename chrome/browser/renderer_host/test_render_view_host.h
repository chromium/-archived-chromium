// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_TEST_RENDER_VIEW_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_TEST_RENDER_VIEW_HOST_H_

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_host/mock_render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/browser/tab_contents/test_web_contents.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "chrome/browser/tab_contents/navigation_controller.h"
#elif defined(OS_POSIX)
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

class TestWebContents;

// This file provides a testing framework for mocking out the RenderProcessHost
// layer. It allows you to test RenderViewHost, WebContents,
// NavigationController, and other layers above that without running an actual
// renderer process.
//
// To use, derive your test base class from RenderViewHostTestHarness.

// TestRenderViewHostView ------------------------------------------------------

// Subclass the RenderViewHost's view so that we can call Show(), etc.,
// without having side-effects.
class TestRenderWidgetHostView : public RenderWidgetHostView {
 public:
  TestRenderWidgetHostView() : is_showing_(false) {}

  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) {}
  virtual RenderWidgetHost* GetRenderWidgetHost() const { return NULL; }
  virtual void DidBecomeSelected() {}
  virtual void WasHidden() {}
  virtual void SetSize(const gfx::Size& size) {}
  virtual gfx::NativeView GetPluginNativeView() { return NULL; }
  virtual void MovePluginWindows(
    const std::vector<WebPluginGeometry>& plugin_window_moves) {}
#if defined(OS_WIN)
  virtual void ForwardMouseEventToRenderer(UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam) {}
#endif
  virtual void Focus() {}
  virtual void Blur() {}
  virtual bool HasFocus() { return true; }
  virtual void AdvanceFocus(bool reverse) {}
  virtual void Show() { is_showing_ = true; }
  virtual void Hide() { is_showing_ = false; }
  virtual gfx::Rect GetViewBounds() const { return gfx::Rect(); }
  virtual void SetIsLoading(bool is_loading) {}
  virtual void UpdateCursor(const WebCursor& cursor) {}
  virtual void UpdateCursorIfOverSelf() {}
  virtual void IMEUpdateStatus(int control, const gfx::Rect& caret_rect) {}
  virtual void DidPaintRect(const gfx::Rect& rect) {}
  virtual void DidScrollRect(const gfx::Rect& rect, int dx, int dy) {}
  virtual void RenderViewGone() {}
  virtual void Destroy() {}
  virtual void PrepareToDestroy() {}
  virtual void SetTooltipText(const std::wstring& tooltip_text) {}
  virtual BackingStore* AllocBackingStore(const gfx::Size& size);

  bool is_showing() const { return is_showing_; }

 private:
  bool is_showing_;
};

// TestRenderViewHost ----------------------------------------------------------

// TODO(brettw) this should use a TestWebContents which should be generalized
// from the WebContents test. We will probably also need that class' version of
// CreateRenderViewForRenderManager when more complicate tests start using this.
class TestRenderViewHost : public RenderViewHost {
 public:
  TestRenderViewHost(SiteInstance* instance,
                     RenderViewHostDelegate* delegate,
                     int routing_id,
                     base::WaitableEvent* modal_dialog_event);
  virtual ~TestRenderViewHost();

  // Testing functions ---------------------------------------------------------

  // Calls the RenderViewHosts' private OnMessageReceived function with the
  // given message.
  void TestOnMessageReceived(const IPC::Message& msg);

  // Calls OnMsgNavigate on the RenderViewHost with the given information,
  // setting the rest of the parameters in the message to the "typical" values.
  // This is a helper function for simulating the most common types of loads.
  void SendNavigate(int page_id, const GURL& url);

  // If set, *delete_counter is incremented when this object destructs.
  void set_delete_counter(int* delete_counter) {
    delete_counter_ = delete_counter;
  }

  // Sets whether the RenderView currently exists or not. This controls the
  // return value from IsRenderViewLive, which the rest of the system uses to
  // check whether the RenderView has crashed or not.
  void set_render_view_created(bool created) {
    render_view_created_ = created;
  }

  // RenderViewHost overrides --------------------------------------------------

  virtual bool CreateRenderView();
  virtual bool IsRenderViewLive() const;

 private:
  FRIEND_TEST(RenderViewHostTest, FilterNavigate);

  // Tracks if the caller thinks if it created the RenderView. This is so we can
  // respond to IsRenderViewLive appropriately.
  bool render_view_created_;

  // See set_delete_counter() above. May be NULL.
  int* delete_counter_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewHost);
};

// TestRenderViewHostFactory ---------------------------------------------------

class TestRenderViewHostFactory : public RenderViewHostFactory {
 public:
  TestRenderViewHostFactory(RenderProcessHostFactory* rph_factory)
      : render_process_host_factory_(rph_factory) {
  }
  virtual ~TestRenderViewHostFactory() {
  }

  virtual RenderViewHost* CreateRenderViewHost(
      SiteInstance* instance,
      RenderViewHostDelegate* delegate,
      int routing_id,
      base::WaitableEvent* modal_dialog_event) {
    // See declaration of render_process_host_factory_ below.
    instance->set_render_process_host_factory(render_process_host_factory_);
    return new TestRenderViewHost(instance, delegate, routing_id,
                                  modal_dialog_event);
  }

 private:
  // This is a bit of a hack. With the current design of the site instances /
  // browsing instances, it's difficult to pass a RenderProcessHostFactory
  // around properly.
  //
  // Instead, we set it right before we create a new RenderViewHost, which
  // happens before the RenderProcessHost is created. This way, the instance
  // has the correct factory and creates our special RenderProcessHosts.
  RenderProcessHostFactory* render_process_host_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewHostFactory);
};

// RenderViewHostTestHarness ---------------------------------------------------

class RenderViewHostTestHarness : public testing::Test {
 public:
  RenderViewHostTestHarness()
      : rph_factory_(),
        rvh_factory_(&rph_factory_),
        process_(NULL),
        contents_(NULL),
        controller_(NULL) {}
  virtual ~RenderViewHostTestHarness() {}

  NavigationController* controller() {
    return contents_->controller();
  }

  TestWebContents* contents() {
    return contents_;
  }

  TestRenderViewHost* rvh() {
    return reinterpret_cast<TestRenderViewHost*>(contents_->render_view_host());
  }

  Profile* profile() {
    return profile_.get();
  }

  // Marks the contents as already cleaned up. If a test calls CloseContents,
  // then our cleanup code shouldn't run. This function makes sure that happens.
  void ContentsCleanedUp() {
    contents_ = NULL;
  }

 protected:
  // testing::Test
  virtual void SetUp();
  virtual void TearDown();

  MessageLoopForUI message_loop_;

  // This profile will be created in SetUp if it has not already been created.
  // This allows tests to override the profile if they so choose in their own
  // SetUp function before calling the base class's (us) SetUp().
  scoped_ptr<TestingProfile> profile_;

  MockRenderProcessHostFactory rph_factory_;
  TestRenderViewHostFactory rvh_factory_;

  // We clean up the WebContents by calling CloseContents, which deletes itself.
  // This in turn causes the destruction of these other things.
  MockRenderProcessHost* process_;
  TestWebContents* contents_;
  NavigationController* controller_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestHarness);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_TEST_RENDER_VIEW_HOST_H_
