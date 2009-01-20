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
#include "chrome/browser/render_widget_host_view.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  virtual RenderWidgetHost* GetRenderWidgetHost() const { return NULL; }
  virtual void DidBecomeSelected() {}
  virtual void WasHidden() {}
  virtual void SetSize(const gfx::Size& size) {}
  virtual HWND GetPluginHWND() { return NULL; }
  virtual HANDLE ModalDialogEvent() { return NULL; }
  virtual void ForwardMouseEventToRenderer(UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam) {}
  virtual void Focus() {}
  virtual void Blur() {}
  virtual bool HasFocus() { return true; }
  virtual void AdvanceFocus(bool reverse) {}
  virtual void Show() { is_showing_ = true; }
  virtual void Hide() { is_showing_ = false; }
  virtual gfx::Rect GetViewBounds() const { return gfx::Rect(); }
  virtual void UpdateCursor(const WebCursor& cursor) {}
  virtual void UpdateCursorIfOverSelf() {}
  // Indicates if the page has finished loading.
  virtual void SetIsLoading(bool is_loading) {}
  virtual void IMEUpdateStatus(ViewHostMsg_ImeControl control,
                               const gfx::Rect& caret_rect) {}
  virtual void DidPaintRect(const gfx::Rect& rect) {}
  virtual void DidScrollRect(const gfx::Rect& rect, int dx, int dy) {}
  virtual void RendererGone() {}
  virtual void Destroy() {}
  virtual void PrepareToDestroy() {}
  virtual void SetTooltipText(const std::wstring& tooltip_text) {}
 
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
  TestRenderViewHostFactory() {}
  virtual ~TestRenderViewHostFactory() {}

  static TestRenderViewHostFactory* GetInstance() {
    static TestRenderViewHostFactory instance;
    return &instance;
  }

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

// RenderViewHostTestHarness ---------------------------------------------------

class RenderViewHostTestHarness : public testing::Test {
 public:
  RenderViewHostTestHarness()
      : process_(NULL),
        contents_(NULL),
        controller_(NULL) {}
  virtual ~RenderViewHostTestHarness() {}

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
