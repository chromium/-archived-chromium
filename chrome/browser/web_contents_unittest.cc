// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/interstitial_page.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/render_widget_host_view.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

// Subclass the RenderViewHost's view so that we can call Show(), etc.,
// without having side-effects.
class TestRenderWidgetHostView : public RenderWidgetHostView {
 public:
  TestRenderWidgetHostView() {}

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
  virtual void Show() {}
  virtual void Hide() {}
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
};

// Subclass RenderViewHost so that it does not create a process.
class TestRenderViewHost : public RenderViewHost {
 public:
  TestRenderViewHost(
      SiteInstance* instance,
      RenderViewHostDelegate* delegate,
      int routing_id,
      HANDLE modal_dialog_event)
      : RenderViewHost(instance, delegate, routing_id, modal_dialog_event),
        is_loading(false),
        is_created(false),
        immediate_before_unload(true),
        delete_counter_(NULL) {
    set_view(new TestRenderWidgetHostView());
  }
  ~TestRenderViewHost() {
    // Track the delete if we've been asked to.
    if (delete_counter_)
      ++*delete_counter_;

    // Since this isn't a traditional view, we have to delete it.
    delete view_;
  }

  // If set, *delete_counter is incremented when this object destructs.
  void set_delete_counter(int* delete_counter) {
    delete_counter_ = delete_counter;
  }

  bool CreateRenderView() {
    is_created = true;
    return true;
  }

  bool IsRenderViewLive() const { return is_created; }

  bool IsNavigationSuspended() { return navigations_suspended_; }

  void NavigateToEntry(const NavigationEntry& entry, bool is_reload) {
    is_loading = true;
  }

  void LoadAlternateHTMLString(const std::string& html_text,
                               bool new_navigation,
                               const GURL& display_url,
                               const std::string& security_info) {
    is_loading = true;
  }

  // Support for onbeforeunload, onunload
  void FirePageBeforeUnload() {
    is_waiting_for_unload_ack_ = true;
    if (immediate_before_unload)
      delegate()->ShouldClosePage(true);
  }
  void ClosePage(int new_render_process_host_id, int new_request_id) {
    // Nothing to do here...  This would cause a ClosePage_ACK to be sent to
    // ResourceDispatcherHost, so we can simulate that manually.
  }
  void TestOnMsgShouldClose(bool proceed) {
    OnMsgShouldCloseACK(proceed);
  }

  bool is_loading;
  bool is_created;
  bool immediate_before_unload;
  int* delete_counter_;
};

// Factory to create TestRenderViewHosts.
class TestRenderViewHostFactory : public RenderViewHostFactory {
 public:
  static TestRenderViewHostFactory* GetInstance() {
    static TestRenderViewHostFactory instance;
    return &instance;
  }

  virtual RenderViewHost* CreateRenderViewHost(
      SiteInstance* instance,
      RenderViewHostDelegate* delegate,
      int routing_id,
      HANDLE modal_dialog_event) {
    return new TestRenderViewHost(
        instance, delegate, routing_id, modal_dialog_event);
  }

 private:
  TestRenderViewHostFactory() {}
};

// Subclass the TestingProfile so that it can return certain services we need.
class WebContentsTestingProfile : public TestingProfile {
 public:
  WebContentsTestingProfile() : TestingProfile() { }

  virtual PrefService* GetPrefs() {
    if (!prefs_.get()) {
      std::wstring source_path;
      PathService::Get(chrome::DIR_TEST_DATA, &source_path);
      file_util::AppendToPath(&source_path, L"profiles");
      file_util::AppendToPath(&source_path, L"chrome_prefs");
      file_util::AppendToPath(&source_path, L"Preferences");

      prefs_.reset(new PrefService(source_path));
      Profile::RegisterUserPrefs(prefs_.get());
      browser::RegisterAllPrefs(prefs_.get(), prefs_.get());
    }
    return prefs_.get();
  }
};

// Subclass WebContents to ensure it creates TestRenderViewHosts and does
// not do anything involving views.
class TestWebContents : public WebContents {
 public:
  TestWebContents(Profile* profile, SiteInstance* instance)
    : WebContents(profile,
                  instance,
                  TestRenderViewHostFactory::GetInstance(),
                  MSG_ROUTING_NONE,
                  NULL),
      transition_cross_site(false) {}

  // Accessors for interesting fields
  TestRenderViewHost* rvh() {
    return static_cast<TestRenderViewHost*>(
        render_manager_.render_view_host_);
  }
  TestRenderViewHost* pending_rvh() {
    return static_cast<TestRenderViewHost*>(
        render_manager_.pending_render_view_host_);
  }
  TestRenderViewHost* interstitial_rvh() {
    return static_cast<TestRenderViewHost*>(
        render_manager_.interstitial_render_view_host_);
  }
  TestRenderViewHost* original_rvh() {
    return static_cast<TestRenderViewHost*>(
        render_manager_.original_render_view_host_);
  }

  // State accessors.
  bool state_is_normal() const {
    return render_manager_.renderer_state_ == RenderViewHostManager::NORMAL;
  }
  bool state_is_pending() const {
    return render_manager_.renderer_state_ == RenderViewHostManager::PENDING;
  }
  bool state_is_entering_interstitial() const {
    return render_manager_.renderer_state_ ==
        RenderViewHostManager::ENTERING_INTERSTITIAL;
  }
  bool state_is_interstitial() const {
    return render_manager_.renderer_state_ ==
        RenderViewHostManager::INTERSTITIAL;
  }
  bool state_is_leaving_interstitial() const {
    return render_manager_.renderer_state_ ==
        RenderViewHostManager::LEAVING_INTERSTITIAL;
  }

  // Ensure we create TestRenderViewHosts that don't spawn processes.
  RenderViewHost* CreateRenderViewHost(SiteInstance* instance,
                                       RenderViewHostDelegate* delegate,
                                       int routing_id,
                                       HANDLE modal_dialog_event) {
    return new TestRenderViewHost(
        instance, delegate, routing_id, modal_dialog_event);
  }

  // Overrides WebContents::ShouldTransitionCrossSite so that we can test both
  // alternatives without using command-line switches.
  bool ShouldTransitionCrossSite() { return transition_cross_site; }

  // Promote DidNavigate to public.
  void TestDidNavigate(TestRenderViewHost* render_view_host,
                       const ViewHostMsg_FrameNavigate_Params& params) {
    DidNavigate(render_view_host, params);
    render_view_host->is_loading = false;
  }

  // Promote GetWebkitPrefs to public.
  WebPreferences TestGetWebkitPrefs() {
    return GetWebkitPrefs();
  }

  // Prevent interaction with views.
  bool CreateRenderViewForRenderManager(RenderViewHost* render_view_host) {
    // This will go to a TestRenderViewHost.
    render_view_host->CreateRenderView();
    return true;
  }
  void UpdateRenderViewSizeForRenderManager() {}

  // Set by individual tests.
  bool transition_cross_site;
};

class WebContentsTest : public testing::Test {
 public:
  WebContentsTest() : contents(NULL) {}

  void InitNavigateParams(ViewHostMsg_FrameNavigate_Params* params,
                          int pageID,
                          const GURL& url) {
    params->page_id = pageID;
    params->url = url;
    params->referrer = GURL::EmptyGURL();
    params->transition = PageTransition::TYPED;
    params->redirects = std::vector<GURL>();
    params->should_update_history = false;
    params->searchable_form_url = GURL::EmptyGURL();
    params->searchable_form_element_name = std::wstring();
    params->searchable_form_encoding = std::string();
    params->password_form = PasswordForm();
    params->security_info = std::string();
    params->gesture = NavigationGestureUser;
    params->is_post = false;
  }

  // testing::Test methods:

  virtual void SetUp() {
    profile.reset(new WebContentsTestingProfile());

    // This will be deleted when the WebContents goes away
    SiteInstance* instance = SiteInstance::CreateSiteInstance(profile.get());

    contents = new TestWebContents(profile.get(), instance);
    contents->SetupController(profile.get());
  }

  virtual void TearDown() {
    // This will delete the contents.
    contents->CloseContents();

    // Make sure that we flush any messages related to WebContents destruction
    // before we destroy the profile.
    MessageLoop::current()->RunAllPending();
  }

  scoped_ptr<WebContentsTestingProfile> profile;
  TestWebContents* contents;

 private:
  MessageLoopForUI message_loop_;
};

// Test to make sure that title updates get stripped of whitespace.
TEST_F(WebContentsTest, UpdateTitle) {
  ViewHostMsg_FrameNavigate_Params params;
  InitNavigateParams(&params, 0, GURL("about:blank"));

  NavigationController::LoadCommittedDetails details;
  contents->controller()->RendererDidNavigate(params, false, &details);

  contents->UpdateTitle(NULL, 0, L"    Lots O' Whitespace\n");
  EXPECT_EQ(std::wstring(L"Lots O' Whitespace"), contents->GetTitle());
}

// Test simple same-SiteInstance navigation.
TEST_F(WebContentsTest, SimpleNavigation) {
  TestRenderViewHost* orig_rvh = contents->rvh();
  SiteInstance* instance1 = contents->GetSiteInstance();
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);
  EXPECT_FALSE(orig_rvh->is_loading);

  // Navigate to URL
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_TRUE(orig_rvh->is_loading);
  EXPECT_EQ(instance1, orig_rvh->site_instance());
  // Controller's pending entry will have a NULL site instance until we assign
  // it in DidNavigate.
  EXPECT_TRUE(
    contents->controller()->GetActiveEntry()->site_instance() == NULL);

  // DidNavigate from the page
  ViewHostMsg_FrameNavigate_Params params;
  InitNavigateParams(&params, 1, url);
  contents->TestDidNavigate(orig_rvh, params);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(orig_rvh, contents->render_view_host());
  EXPECT_EQ(instance1, orig_rvh->site_instance());
  // Controller's entry should now have the SiteInstance, or else we won't be
  // able to find it later.
  EXPECT_EQ(instance1,
            contents->controller()->GetActiveEntry()->site_instance());
}

// Test navigating to a page that shows an interstitial, then hiding it
// without proceeding.
TEST_F(WebContentsTest, ShowInterstitialDontProceed) {
  TestRenderViewHost* orig_rvh = contents->rvh();
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);
  EXPECT_FALSE(orig_rvh->is_loading);

  // Navigate to URL
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_TRUE(orig_rvh->is_loading);

  // Show interstitial
  const GURL interstitial_url("http://interstitial");
  InterstitialPage* interstitial = new InterstitialPage(contents,
                                                        true,
                                                        interstitial_url);
  interstitial->Show();
  EXPECT_TRUE(contents->state_is_entering_interstitial());
  TestRenderViewHost* interstitial_rvh = contents->interstitial_rvh();
  EXPECT_TRUE(orig_rvh->is_loading);  // Still loading in the background
  EXPECT_TRUE(interstitial_rvh->is_loading);

  // DidNavigate from the interstitial
  ViewHostMsg_FrameNavigate_Params params;
  InitNavigateParams(&params, 1, url);
  contents->TestDidNavigate(interstitial_rvh, params);
  EXPECT_TRUE(contents->state_is_interstitial());
  EXPECT_EQ(interstitial_rvh, contents->render_view_host());
  EXPECT_EQ(orig_rvh, contents->original_rvh());
  EXPECT_FALSE(interstitial_rvh->is_loading);

  // Hide interstitial (don't proceed)
  contents->HideInterstitialPage(false, false);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(orig_rvh, contents->render_view_host());
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);
}

// Test navigating to a page that shows an interstitial, then proceeding.
TEST_F(WebContentsTest, ShowInterstitialProceed) {
  TestRenderViewHost* orig_rvh = contents->rvh();

  // The RenderViewHost's SiteInstance should not yet have a site.
  EXPECT_EQ(GURL(), contents->rvh()->site_instance()->site());

  // Navigate to URL
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);

  // Show interstitial
  const GURL interstitial_url("http://interstitial");
  InterstitialPage* interstitial = new InterstitialPage(contents,
                                                        true,
                                                        interstitial_url);
  interstitial->Show();
  TestRenderViewHost* interstitial_rvh = contents->interstitial_rvh();

  // DidNavigate from the interstitial
  ViewHostMsg_FrameNavigate_Params params;
  InitNavigateParams(&params, 1, url);
  contents->TestDidNavigate(interstitial_rvh, params);

  // Ensure this DidNavigate hasn't changed the SiteInstance's site.
  // Prevents regression for bug 1163298.
  EXPECT_EQ(GURL(), contents->rvh()->site_instance()->site());

  // Hide interstitial (proceed and wait)
  contents->HideInterstitialPage(true, true);
  EXPECT_TRUE(contents->state_is_leaving_interstitial());
  EXPECT_EQ(interstitial_rvh, contents->render_view_host());
  EXPECT_EQ(orig_rvh, contents->original_rvh());

  // DidNavigate from the destination page
  contents->TestDidNavigate(orig_rvh, params);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(orig_rvh, contents->render_view_host());
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);

  // The SiteInstance's site should now be updated.
  EXPECT_EQ(GURL("http://google.com"),
            contents->rvh()->site_instance()->site());

  // Since we weren't viewing a page before, we shouldn't be able to go back.
  EXPECT_FALSE(contents->controller()->CanGoBack());
}

// Test navigating to a page that shows an interstitial, then navigating away.
TEST_F(WebContentsTest, ShowInterstitialThenNavigate) {
  TestRenderViewHost* orig_rvh = contents->rvh();

  // Navigate to URL
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);

  // Show interstitial
  const GURL interstitial_url("http://interstitial");
  InterstitialPage* interstitial = new InterstitialPage(contents,
                                                        true,
                                                        interstitial_url);
  interstitial->Show();
  TestRenderViewHost* interstitial_rvh = contents->interstitial_rvh();

  // DidNavigate from the interstitial
  ViewHostMsg_FrameNavigate_Params params;
  InitNavigateParams(&params, 1, url);
  contents->TestDidNavigate(interstitial_rvh, params);

  // While interstitial showing, navigate to a new URL.
  const GURL url2("http://www.yahoo.com");
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  EXPECT_TRUE(contents->state_is_leaving_interstitial());
  EXPECT_EQ(interstitial_rvh, contents->render_view_host());
  EXPECT_TRUE(orig_rvh->is_loading);
  EXPECT_FALSE(interstitial_rvh->is_loading);

  // DidNavigate from the new URL.  In the old process model, we'll still have
  // the same RenderViewHost.
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 2, url2);
  contents->TestDidNavigate(orig_rvh, params2);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(orig_rvh, contents->render_view_host());
  EXPECT_FALSE(orig_rvh->is_loading);
}

// Ensures that an interstitial cannot be cancelled if a notification for a
// navigation from an IFrame from the previous page is received while the
// interstitial is being shown (bug #1182394).
TEST_F(WebContentsTest, ShowInterstitialIFrameNavigate) {
  TestRenderViewHost* orig_rvh = contents->rvh();
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);
  EXPECT_FALSE(orig_rvh->is_loading);

  // Navigate to URL.
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_TRUE(orig_rvh->is_loading);
  ViewHostMsg_FrameNavigate_Params params1;
  InitNavigateParams(&params1, 1, url);
  contents->TestDidNavigate(orig_rvh, params1);

  // Show interstitial (in real world would probably be triggered by a resource
  // in the page).
  const GURL interstitial_url("http://interstitial");
  InterstitialPage* interstitial = new InterstitialPage(contents,
                                                        true,
                                                        interstitial_url);
  interstitial->Show();
  EXPECT_TRUE(contents->state_is_entering_interstitial());
  TestRenderViewHost* interstitial_rvh = contents->interstitial_rvh();
  EXPECT_TRUE(interstitial_rvh->is_loading);

  // DidNavigate from an IFrame in the initial page.
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 1, GURL("http://www.iframe.com"));
  params2.transition = PageTransition::AUTO_SUBFRAME;
  contents->TestDidNavigate(orig_rvh, params2);

  // Now we get the DidNavigate from the interstitial.
  ViewHostMsg_FrameNavigate_Params params3;
  InitNavigateParams(&params3, 1, url);
  contents->TestDidNavigate(interstitial_rvh, params3);
  EXPECT_TRUE(contents->state_is_interstitial());
  EXPECT_EQ(interstitial_rvh, contents->render_view_host());
  EXPECT_EQ(orig_rvh, contents->original_rvh());
  EXPECT_FALSE(interstitial_rvh->is_loading);
}

// Test navigating to an interstitial page from a normal page.  Also test
// visiting the interstitial-inducing URL twice (bug 1079784), and test
// that going back shows the first page and not the interstitial.
TEST_F(WebContentsTest, VisitInterstitialURLTwice) {
  TestRenderViewHost* orig_rvh = contents->rvh();

  // Navigate to URL
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  ViewHostMsg_FrameNavigate_Params params1;
  InitNavigateParams(&params1, 1, url);
  contents->TestDidNavigate(orig_rvh, params1);

  // Now navigate to an interstitial-inducing URL
  const GURL url2("https://www.google.com");
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  const GURL interstitial_url("http://interstitial");
  InterstitialPage* interstitial = new InterstitialPage(contents,
                                                        true,
                                                        interstitial_url);
  interstitial->Show();
  EXPECT_TRUE(contents->state_is_entering_interstitial());
  int interstitial_delete_counter = 0;
  TestRenderViewHost* interstitial_rvh = contents->interstitial_rvh();
  interstitial_rvh->set_delete_counter(&interstitial_delete_counter);

  // DidNavigate from the interstitial
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 2, url2);
  contents->TestDidNavigate(interstitial_rvh, params2);
  EXPECT_TRUE(contents->state_is_interstitial());
  EXPECT_EQ(interstitial_rvh, contents->render_view_host());

  // While interstitial showing, navigate to the same URL.
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  EXPECT_TRUE(contents->state_is_leaving_interstitial());
  EXPECT_EQ(interstitial_rvh, contents->render_view_host());

  // Interstitial shown a second time in a different RenderViewHost.
  interstitial = new InterstitialPage(contents, true, interstitial_url);
  interstitial->Show();
  EXPECT_TRUE(contents->state_is_entering_interstitial());
  // We expect the original interstitial has been deleted.
  EXPECT_EQ(interstitial_delete_counter, 1);
  TestRenderViewHost* interstitial_rvh2 = contents->interstitial_rvh();
  interstitial_rvh2->set_delete_counter(&interstitial_delete_counter);

  // DidNavigate from the interstitial.
  ViewHostMsg_FrameNavigate_Params params3;
  InitNavigateParams(&params3, 3, url2);
  contents->TestDidNavigate(interstitial_rvh2, params3);
  EXPECT_TRUE(contents->state_is_interstitial());
  EXPECT_EQ(interstitial_rvh2, contents->render_view_host());

  // Proceed.  In the old process model, we'll still have the same
  // RenderViewHost.
  contents->HideInterstitialPage(true, true);
  EXPECT_TRUE(contents->state_is_leaving_interstitial());
  ViewHostMsg_FrameNavigate_Params params4;
  InitNavigateParams(&params4, 3, url2);
  contents->TestDidNavigate(orig_rvh, params4);
  EXPECT_TRUE(contents->state_is_normal());
  // We expect the second interstitial has been deleted.
  EXPECT_EQ(interstitial_delete_counter, 2);

  // Now go back.  Should take us back to the original page.
  contents->controller()->GoBack();
  EXPECT_TRUE(contents->state_is_normal());

  // DidNavigate from going back.
  contents->TestDidNavigate(orig_rvh, params1);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(orig_rvh, contents->render_view_host());
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);
}

// Test that navigating across a site boundary creates a new RenderViewHost
// with a new SiteInstance.  Going back should do the same.
TEST_F(WebContentsTest, CrossSiteBoundaries) {
  contents->transition_cross_site = true;
  TestRenderViewHost* orig_rvh = contents->rvh();
  int orig_rvh_delete_count = 0;
  orig_rvh->set_delete_counter(&orig_rvh_delete_count);
  SiteInstance* instance1 = contents->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  ViewHostMsg_FrameNavigate_Params params1;
  InitNavigateParams(&params1, 1, url);
  contents->TestDidNavigate(orig_rvh, params1);

  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(orig_rvh, contents->render_view_host());
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);

  // Navigate to new site
  const GURL url2("http://www.yahoo.com");
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  EXPECT_TRUE(contents->state_is_pending());
  TestRenderViewHost* pending_rvh = contents->pending_rvh();
  int pending_rvh_delete_count = 0;
  pending_rvh->set_delete_counter(&pending_rvh_delete_count);

  // DidNavigate from the pending page
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 1, url2);
  contents->TestDidNavigate(pending_rvh, params2);
  SiteInstance* instance2 = contents->GetSiteInstance();

  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(pending_rvh, contents->render_view_host());
  EXPECT_NE(instance1, instance2);
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);
  EXPECT_EQ(orig_rvh_delete_count, 1);

  // Going back should switch SiteInstances again.  The first SiteInstance is
  // stored in the NavigationEntry, so it should be the same as at the start.
  contents->controller()->GoBack();
  TestRenderViewHost* goback_rvh = contents->pending_rvh();
  EXPECT_TRUE(contents->state_is_pending());

  // DidNavigate from the back action
  contents->TestDidNavigate(goback_rvh, params1);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(goback_rvh, contents->render_view_host());
  EXPECT_EQ(pending_rvh_delete_count, 1);
  EXPECT_EQ(instance1, contents->GetSiteInstance());
}

// Test that navigating across a site boundary after a crash creates a new
// RVH without requiring a cross-site transition (i.e., PENDING state).
TEST_F(WebContentsTest, CrossSiteBoundariesAfterCrash) {
  contents->transition_cross_site = true;
  TestRenderViewHost* orig_rvh = contents->rvh();
  int orig_rvh_delete_count = 0;
  orig_rvh->set_delete_counter(&orig_rvh_delete_count);
  SiteInstance* instance1 = contents->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  ViewHostMsg_FrameNavigate_Params params1;
  InitNavigateParams(&params1, 1, url);
  contents->TestDidNavigate(orig_rvh, params1);

  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(orig_rvh, contents->render_view_host());
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);

  // Crash the renderer.
  orig_rvh->is_created = false;

  // Navigate to new site.  We should not go into PENDING.
  const GURL url2("http://www.yahoo.com");
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  TestRenderViewHost* new_rvh = contents->rvh();
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);
  EXPECT_NE(orig_rvh, new_rvh);
  EXPECT_EQ(orig_rvh_delete_count, 1);

  // DidNavigate from the new page
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 1, url2);
  contents->TestDidNavigate(new_rvh, params2);
  SiteInstance* instance2 = contents->GetSiteInstance();

  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(new_rvh, contents->render_view_host());
  EXPECT_NE(instance1, instance2);
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);
}

// Test state transitions when showing an interstitial in the new process
// model, and then choosing DontProceed.
TEST_F(WebContentsTest, CrossSiteInterstitialDontProceed) {
  contents->transition_cross_site = true;
  TestRenderViewHost* orig_rvh = contents->rvh();
  SiteInstance* instance1 = contents->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  ViewHostMsg_FrameNavigate_Params params1;
  InitNavigateParams(&params1, 1, url);
  contents->TestDidNavigate(orig_rvh, params1);

  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(orig_rvh, contents->render_view_host());

  // Navigate to new site
  const GURL url2("https://www.google.com");
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  EXPECT_TRUE(contents->state_is_pending());
  TestRenderViewHost* pending_rvh = contents->pending_rvh();

  // Show an interstitial
  const GURL interstitial_url("http://interstitial");
  InterstitialPage* interstitial = new InterstitialPage(contents,
                                                        true,
                                                        interstitial_url);
  interstitial->Show();
  EXPECT_TRUE(contents->state_is_entering_interstitial());
  EXPECT_EQ(orig_rvh, contents->render_view_host());
  EXPECT_EQ(pending_rvh, contents->pending_rvh());
  TestRenderViewHost* interstitial_rvh = contents->interstitial_rvh();

  // DidNavigate from the interstitial
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 2, url2);
  contents->TestDidNavigate(interstitial_rvh, params2);
  EXPECT_TRUE(contents->state_is_interstitial());
  EXPECT_EQ(interstitial_rvh, contents->render_view_host());
  EXPECT_EQ(orig_rvh, contents->original_rvh());
  EXPECT_EQ(pending_rvh, contents->pending_rvh());
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);

  // Hide interstitial (don't proceed)
  contents->HideInterstitialPage(false, false);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(orig_rvh, contents->render_view_host());
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);
}

// Test state transitions when showing an interstitial in the new process
// model, and then choosing Proceed.
TEST_F(WebContentsTest, CrossSiteInterstitialProceed) {
  contents->transition_cross_site = true;
  int orig_rvh_delete_count = 0;
  TestRenderViewHost* orig_rvh = contents->rvh();
  orig_rvh->set_delete_counter(&orig_rvh_delete_count);
  SiteInstance* instance1 = contents->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  ViewHostMsg_FrameNavigate_Params params1;
  InitNavigateParams(&params1, 1, url);
  contents->TestDidNavigate(orig_rvh, params1);

  // Navigate to new site
  const GURL url2("https://www.google.com");
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  TestRenderViewHost* pending_rvh = contents->pending_rvh();
  int pending_rvh_delete_count = 0;
  pending_rvh->set_delete_counter(&pending_rvh_delete_count);

  // Show an interstitial
  const GURL interstitial_url("http://interstitial");
  InterstitialPage* interstitial = new InterstitialPage(contents,
                                                        true,
                                                        interstitial_url);
  interstitial->Show();
  TestRenderViewHost* interstitial_rvh = contents->interstitial_rvh();

  // DidNavigate from the interstitial
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 1, url2);
  contents->TestDidNavigate(interstitial_rvh, params2);
  EXPECT_TRUE(contents->state_is_interstitial());
  EXPECT_EQ(interstitial_rvh, contents->render_view_host());
  EXPECT_EQ(orig_rvh, contents->original_rvh());
  EXPECT_EQ(pending_rvh, contents->pending_rvh());
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);

  // Hide interstitial (proceed and wait)
  contents->HideInterstitialPage(true, true);
  EXPECT_TRUE(contents->state_is_leaving_interstitial());
  EXPECT_EQ(interstitial_rvh, contents->render_view_host());
  EXPECT_EQ(orig_rvh, contents->original_rvh());
  EXPECT_EQ(pending_rvh, contents->pending_rvh());
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);

  // DidNavigate from the destination page should transition to new renderer
  ViewHostMsg_FrameNavigate_Params params3;
  InitNavigateParams(&params3, 2, url2);
  contents->TestDidNavigate(pending_rvh, params3);
  SiteInstance* instance2 = contents->GetSiteInstance();
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(pending_rvh, contents->render_view_host());
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);
  EXPECT_NE(instance1, instance2);
  EXPECT_EQ(orig_rvh_delete_count, 1);  // The original should be gone.

  // Since we were viewing a page before, we should be able to go back.
  EXPECT_TRUE(contents->controller()->CanGoBack());

  // Going back should switch SiteInstances again.  The first SiteInstance is
  // stored in the NavigationEntry, so it should be the same as at the start.
  contents->controller()->GoBack();
  TestRenderViewHost* goback_rvh = contents->pending_rvh();
  EXPECT_TRUE(contents->state_is_pending());

  // DidNavigate from the back action
  contents->TestDidNavigate(goback_rvh, params1);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(goback_rvh, contents->render_view_host());
  EXPECT_EQ(instance1, contents->GetSiteInstance());
  EXPECT_EQ(pending_rvh_delete_count, 1);  // The second page's rvh should die.
}

// Tests that we can transition away from an interstitial page.
TEST_F(WebContentsTest, CrossSiteInterstitialThenNavigate) {
  contents->transition_cross_site = true;
  int orig_rvh_delete_count = 0;
  TestRenderViewHost* orig_rvh = contents->rvh();
  orig_rvh->set_delete_counter(&orig_rvh_delete_count);

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  ViewHostMsg_FrameNavigate_Params params1;
  InitNavigateParams(&params1, 1, url);
  contents->TestDidNavigate(orig_rvh, params1);

  // Show an interstitial
  const GURL interstitial_url("http://interstitial");
  InterstitialPage* interstitial = new InterstitialPage(contents,
                                                        false,
                                                        interstitial_url);
  interstitial->Show();
  TestRenderViewHost* interstitial_rvh = contents->interstitial_rvh();

  // DidNavigate from the interstitial
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 1, url);
  contents->TestDidNavigate(interstitial_rvh, params2);
  EXPECT_TRUE(contents->state_is_interstitial());
  EXPECT_EQ(interstitial_rvh, contents->render_view_host());
  EXPECT_EQ(orig_rvh, contents->original_rvh());
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);

  // Navigate to a new page.
  const GURL url2("http://www.yahoo.com");
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);

  TestRenderViewHost* new_rvh = contents->pending_rvh();
  ASSERT_TRUE(new_rvh != NULL);
  // Make sure the RVH is not suspended (bug #1236441).
  EXPECT_FALSE(new_rvh->IsNavigationSuspended());
  EXPECT_TRUE(contents->state_is_leaving_interstitial());
  EXPECT_EQ(interstitial_rvh, contents->render_view_host());

  // DidNavigate from the new page
  ViewHostMsg_FrameNavigate_Params params3;
  InitNavigateParams(&params3, 1, url2);
  contents->TestDidNavigate(new_rvh, params3);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(new_rvh, contents->render_view_host());
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_EQ(orig_rvh_delete_count, 1);
}

// Tests that we can transition away from an interstitial page even if the
// interstitial renderer has crashed.
TEST_F(WebContentsTest, CrossSiteInterstitialCrashThenNavigate) {
  contents->transition_cross_site = true;
  int orig_rvh_delete_count = 0;
  TestRenderViewHost* orig_rvh = contents->rvh();
  orig_rvh->set_delete_counter(&orig_rvh_delete_count);

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  ViewHostMsg_FrameNavigate_Params params1;
  InitNavigateParams(&params1, 1, url);
  contents->TestDidNavigate(orig_rvh, params1);

  // Navigate to new site
  const GURL url2("https://www.google.com");
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  TestRenderViewHost* pending_rvh = contents->pending_rvh();
  int pending_rvh_delete_count = 0;
  pending_rvh->set_delete_counter(&pending_rvh_delete_count);

  // Show an interstitial
  const GURL interstitial_url("http://interstitial");
  InterstitialPage* interstitial = new InterstitialPage(contents,
                                                        true,
                                                        interstitial_url);
  interstitial->Show();
  TestRenderViewHost* interstitial_rvh = contents->interstitial_rvh();

  // DidNavigate from the interstitial
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 1, url2);
  contents->TestDidNavigate(interstitial_rvh, params2);
  EXPECT_TRUE(contents->state_is_interstitial());
  EXPECT_EQ(interstitial_rvh, contents->render_view_host());
  EXPECT_EQ(orig_rvh, contents->original_rvh());
  EXPECT_EQ(pending_rvh, contents->pending_rvh());
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);

  // Crash the interstitial RVH
  // (by making IsRenderViewLive() == false)
  interstitial_rvh->is_created = false;

  // Navigate to a new page.  Since interstitial RVH is dead, we should clean
  // it up and go to a new PENDING state, showing the orig_rvh.
  const GURL url3("http://www.yahoo.com");
  contents->controller()->LoadURL(url3, GURL(), PageTransition::TYPED);
  TestRenderViewHost* new_rvh = contents->pending_rvh();
  ASSERT_TRUE(new_rvh != NULL);
  EXPECT_TRUE(contents->state_is_pending());
  EXPECT_EQ(orig_rvh, contents->render_view_host());
  EXPECT_EQ(pending_rvh_delete_count, 1);
  EXPECT_NE(interstitial_rvh, new_rvh);
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);

  // DidNavigate from the new page
  ViewHostMsg_FrameNavigate_Params params3;
  InitNavigateParams(&params3, 1, url3);
  contents->TestDidNavigate(new_rvh, params3);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(new_rvh, contents->render_view_host());
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_EQ(orig_rvh_delete_count, 1);
}

// Tests that we can transition away from an interstitial page even if both the
// original and interstitial renderers have crashed.
TEST_F(WebContentsTest, CrossSiteInterstitialCrashesThenNavigate) {
  contents->transition_cross_site = true;
  int orig_rvh_delete_count = 0;
  TestRenderViewHost* orig_rvh = contents->rvh();
  orig_rvh->set_delete_counter(&orig_rvh_delete_count);

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  ViewHostMsg_FrameNavigate_Params params1;
  InitNavigateParams(&params1, 1, url);
  contents->TestDidNavigate(orig_rvh, params1);

  // Navigate to new site
  const GURL url2("https://www.google.com");
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  TestRenderViewHost* pending_rvh = contents->pending_rvh();
  int pending_rvh_delete_count = 0;
  pending_rvh->set_delete_counter(&pending_rvh_delete_count);

  // Show an interstitial
  const GURL interstitial_url("http://interstitial");
  InterstitialPage* interstitial = new InterstitialPage(contents,
                                                        true,
                                                        interstitial_url);
  interstitial->Show();
  TestRenderViewHost* interstitial_rvh = contents->interstitial_rvh();

  // DidNavigate from the interstitial
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 1, url2);
  contents->TestDidNavigate(interstitial_rvh, params2);
  EXPECT_TRUE(contents->state_is_interstitial());
  EXPECT_EQ(interstitial_rvh, contents->render_view_host());
  EXPECT_EQ(orig_rvh, contents->original_rvh());
  EXPECT_EQ(pending_rvh, contents->pending_rvh());
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);

  // Crash both the original and interstitial RVHs
  // (by making IsRenderViewLive() == false)
  orig_rvh->is_created = false;
  interstitial_rvh->is_created = false;

  // Navigate to a new page.  Since both the interstitial and original RVHs are
  // dead, we should create a new RVH, jump back to NORMAL, and navigate.
  const GURL url3("http://www.yahoo.com");
  contents->controller()->LoadURL(url3, GURL(), PageTransition::TYPED);
  TestRenderViewHost* new_rvh = contents->rvh();
  ASSERT_TRUE(new_rvh != NULL);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(orig_rvh_delete_count, 1);
  EXPECT_EQ(pending_rvh_delete_count, 1);
  EXPECT_NE(interstitial_rvh, new_rvh);
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);

  // DidNavigate from the new page
  ViewHostMsg_FrameNavigate_Params params3;
  InitNavigateParams(&params3, 1, url3);
  contents->TestDidNavigate(new_rvh, params3);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(new_rvh, contents->render_view_host());
}

// Test that opening a new tab in the same SiteInstance and then navigating
// both tabs to a new site will place both tabs in a single SiteInstance.
TEST_F(WebContentsTest, NavigateTwoTabsCrossSite) {
  contents->transition_cross_site = true;
  TestRenderViewHost* orig_rvh = contents->rvh();
  SiteInstance* instance1 = contents->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  ViewHostMsg_FrameNavigate_Params params1;
  InitNavigateParams(&params1, 1, url);
  contents->TestDidNavigate(orig_rvh, params1);

  // Open a new tab with the same SiteInstance, navigated to the same site.
  TestWebContents* contents2 = new TestWebContents(profile.get(), instance1);
  params1.page_id = 2;  // Need this since the site instance is the same (which
                        // is the scope of page IDs) and we want to consider
                        // this a new page.
  contents2->transition_cross_site = true;
  contents2->SetupController(profile.get());
  contents2->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  contents2->TestDidNavigate(contents2->rvh(), params1);

  // Navigate first tab to a new site
  const GURL url2a("http://www.yahoo.com");
  contents->controller()->LoadURL(url2a, GURL(), PageTransition::TYPED);
  TestRenderViewHost* pending_rvh_a = contents->pending_rvh();
  ViewHostMsg_FrameNavigate_Params params2a;
  InitNavigateParams(&params2a, 1, url2a);
  contents->TestDidNavigate(pending_rvh_a, params2a);
  SiteInstance* instance2a = contents->GetSiteInstance();
  EXPECT_NE(instance1, instance2a);

  // Navigate second tab to the same site as the first tab
  const GURL url2b("http://mail.yahoo.com");
  contents2->controller()->LoadURL(url2b, GURL(), PageTransition::TYPED);
  TestRenderViewHost* pending_rvh_b = contents2->pending_rvh();
  EXPECT_TRUE(pending_rvh_b != NULL);
  EXPECT_TRUE(contents2->state_is_pending());

  // NOTE(creis): We used to be in danger of showing a sad tab page here if the
  // second tab hadn't navigated somewhere first (bug 1145430).  That case is
  // now covered by the CrossSiteBoundariesAfterCrash test.

  ViewHostMsg_FrameNavigate_Params params2b;
  InitNavigateParams(&params2b, 2, url2b);
  contents2->TestDidNavigate(pending_rvh_b, params2b);
  SiteInstance* instance2b = contents2->GetSiteInstance();
  EXPECT_NE(instance1, instance2b);

  // Both tabs should now be in the same SiteInstance.
  EXPECT_EQ(instance2a, instance2b);

  contents2->CloseContents();
}

// Tests that WebContents uses the current URL, not the SiteInstance's site, to
// determine whether a navigation is cross-site.
TEST_F(WebContentsTest, CrossSiteComparesAgainstCurrentPage) {
  contents->transition_cross_site = true;
  TestRenderViewHost* orig_rvh = contents->rvh();
  SiteInstance* instance1 = contents->GetSiteInstance();

  // Navigate to URL.
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  ViewHostMsg_FrameNavigate_Params params1;
  InitNavigateParams(&params1, 1, url);
  contents->TestDidNavigate(orig_rvh, params1);

  // Open a related tab to a second site.
  TestWebContents* contents2 = new TestWebContents(profile.get(), instance1);
  contents2->transition_cross_site = true;
  contents2->SetupController(profile.get());
  const GURL url2("http://www.yahoo.com");
  contents2->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  // The first RVH in contents2 isn't live yet, so we shortcut the PENDING
  // state and go straight to NORMAL.
  TestRenderViewHost* rvh2 = contents2->rvh();
  EXPECT_TRUE(contents2->state_is_normal());
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 2, url2);
  contents2->TestDidNavigate(rvh2, params2);
  SiteInstance* instance2 = contents2->GetSiteInstance();
  EXPECT_NE(instance1, instance2);
  EXPECT_TRUE(contents2->state_is_normal());

  // Simulate a link click in first tab to second site.  Doesn't switch
  // SiteInstances, because we don't intercept WebKit navigations.
  ViewHostMsg_FrameNavigate_Params params3;
  InitNavigateParams(&params3, 2, url2);
  contents->TestDidNavigate(orig_rvh, params3);
  SiteInstance* instance3 = contents->GetSiteInstance();
  EXPECT_EQ(instance1, instance3);
  EXPECT_TRUE(contents->state_is_normal());

  // Navigate to the new site.  Doesn't switch SiteInstancees, because we
  // compare against the current URL, not the SiteInstance's site.
  const GURL url3("http://mail.yahoo.com");
  contents->controller()->LoadURL(url3, GURL(), PageTransition::TYPED);
  EXPECT_TRUE(contents->state_is_normal());
  ViewHostMsg_FrameNavigate_Params params4;
  InitNavigateParams(&params4, 3, url3);
  contents->TestDidNavigate(orig_rvh, params4);
  SiteInstance* instance4 = contents->GetSiteInstance();
  EXPECT_EQ(instance1, instance4);

  contents2->CloseContents();
}

// Test that the onbeforeunload and onunload handlers run when navigating
// across site boundaries.
TEST_F(WebContentsTest, CrossSiteUnloadHandlers) {
  contents->transition_cross_site = true;
  TestRenderViewHost* orig_rvh = contents->rvh();
  SiteInstance* instance1 = contents->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  ViewHostMsg_FrameNavigate_Params params1;
  InitNavigateParams(&params1, 1, url);
  contents->TestDidNavigate(orig_rvh, params1);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(orig_rvh, contents->render_view_host());

  // Navigate to new site, but simulate an onbeforeunload denial.
  const GURL url2("http://www.yahoo.com");
  orig_rvh->immediate_before_unload = false;
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  orig_rvh->TestOnMsgShouldClose(false);
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(orig_rvh, contents->render_view_host());

  // Navigate again, but simulate an onbeforeunload approval.
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  orig_rvh->TestOnMsgShouldClose(true);
  EXPECT_TRUE(contents->state_is_pending());
  TestRenderViewHost* pending_rvh = contents->pending_rvh();

  // We won't hear DidNavigate until the onunload handler has finished running.
  // (No way to simulate that here, but it involves a call from RDH to
  // WebContents::OnCrossSiteResponse.)

  // DidNavigate from the pending page
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 1, url2);
  contents->TestDidNavigate(pending_rvh, params2);
  SiteInstance* instance2 = contents->GetSiteInstance();
  EXPECT_TRUE(contents->state_is_normal());
  EXPECT_EQ(pending_rvh, contents->render_view_host());
  EXPECT_NE(instance1, instance2);
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_TRUE(contents->original_rvh() == NULL);
  EXPECT_TRUE(contents->interstitial_rvh() == NULL);
}

// Test that NavigationEntries have the correct content state after going
// forward and back.  Prevents regression for bug 1116137.
TEST_F(WebContentsTest, NavigationEntryContentState) {
  TestRenderViewHost* orig_rvh = contents->rvh();

  // Navigate to URL.  There should be no committed entry yet.
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  NavigationEntry* entry = contents->controller()->GetLastCommittedEntry();
  EXPECT_TRUE(entry == NULL);

  // Committed entry should have content state after DidNavigate.
  ViewHostMsg_FrameNavigate_Params params1;
  InitNavigateParams(&params1, 1, url);
  contents->TestDidNavigate(orig_rvh, params1);
  entry = contents->controller()->GetLastCommittedEntry();
  EXPECT_FALSE(entry->content_state().empty());

  // Navigate to same site.
  const GURL url2("http://images.google.com");
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  entry = contents->controller()->GetLastCommittedEntry();
  EXPECT_FALSE(entry->content_state().empty());

  // Committed entry should have content state after DidNavigate.
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 2, url2);
  contents->TestDidNavigate(orig_rvh, params2);
  entry = contents->controller()->GetLastCommittedEntry();
  EXPECT_FALSE(entry->content_state().empty());

  // Now go back.  Committed entry should still have content state.
  contents->controller()->GoBack();
  contents->TestDidNavigate(orig_rvh, params1);
  entry = contents->controller()->GetLastCommittedEntry();
  EXPECT_FALSE(entry->content_state().empty());
}

// Test that NavigationEntries have the correct content state after opening
// a new window to about:blank.  Prevents regression for bug 1116137.
TEST_F(WebContentsTest, NavigationEntryContentStateNewWindow) {
  TestRenderViewHost* orig_rvh = contents->rvh();

  // When opening a new window, it is navigated to about:blank internally.
  // Currently, this results in two DidNavigate events.
  const GURL url("about:blank");
  ViewHostMsg_FrameNavigate_Params params1;
  InitNavigateParams(&params1, 1, url);
  contents->TestDidNavigate(orig_rvh, params1);
  contents->TestDidNavigate(orig_rvh, params1);

  // Should have a content state here.
  NavigationEntry* entry = contents->controller()->GetLastCommittedEntry();
  EXPECT_FALSE(entry->content_state().empty());
}

// Tests to see that webkit preferences are properly loaded and copied over
// to a WebPreferences object.
TEST_F(WebContentsTest, WebKitPrefs) {
  WebPreferences webkit_prefs = contents->TestGetWebkitPrefs();

  // These values have been overridden by the profile preferences.
  EXPECT_EQ(L"UTF-8", webkit_prefs.default_encoding);
  EXPECT_EQ(20, webkit_prefs.default_font_size);
  EXPECT_EQ(false, webkit_prefs.text_areas_are_resizable);
  EXPECT_EQ(true, webkit_prefs.uses_universal_detector);

  // These should still be the default values.
  EXPECT_EQ(L"Times New Roman", webkit_prefs.standard_font_family);
  EXPECT_EQ(true, webkit_prefs.javascript_enabled);
}

