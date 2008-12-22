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

static void InitNavigateParams(ViewHostMsg_FrameNavigate_Params* params,
                               int page_id,
                               const GURL& url) {
  params->page_id = page_id;
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
  
  // State accessor.
  bool cross_navigation_pending() {
    return render_manager_.cross_navigation_pending_;
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

class TestInterstitialPage : public InterstitialPage {
 public:
  enum InterstitialState {
    UNDECIDED = 0, // No decision taken yet.
    OKED,          // Proceed was called.
    CANCELED       // DontProceed was called.
  };

  class Delegate {
   public:
    virtual void TestInterstitialPageDeleted(
        TestInterstitialPage* interstitial) = 0;
  };

  // IMPORTANT NOTE: if you pass stack allocated values for |state| and
  // |deleted| (like all interstitial related tests do at this point), make sure
  // to create an instance of the TestInterstitialPageStateGuard class on the
  // stack in your test.  This will ensure that the TestInterstitialPage states
  // are cleared when the test finishes.
  // Not doing so will cause stack trashing if your test does not hide the
  // interstitial, as in such a case it will be destroyed in the test TearDown
  // method and will dereference the |deleted| local variable which by then is
  // out of scope.
  TestInterstitialPage(WebContents* tab,
                       bool new_navigation,
                       const GURL& url,
                       InterstitialState* state,
                       bool* deleted)
      : InterstitialPage(tab, new_navigation, url),
        state_(state),
        deleted_(deleted),
        command_received_count_(0),
        delegate_(NULL) {
    *state_ = UNDECIDED;
    *deleted_ = false;
  }

  virtual ~TestInterstitialPage() {
    if (deleted_)
      *deleted_ = true;
    if (delegate_)
      delegate_->TestInterstitialPageDeleted(this);
  }

  virtual void DontProceed() {
    if (state_)
      *state_ = CANCELED;
    InterstitialPage::DontProceed();
  }
  virtual void Proceed() {
    if (state_)
      *state_ = OKED;
    InterstitialPage::Proceed();
  }

  int command_received_count() const {
    return command_received_count_;
  }

  void TestDomOperationResponse(const std::string& json_string) {
    DomOperationResponse(json_string, 1);
  }

  void TestDidNavigate(int page_id, const GURL& url) {
    ViewHostMsg_FrameNavigate_Params params;
    InitNavigateParams(&params, page_id, url);
    DidNavigate(render_view_host(), params);
  }

  void TestRendererGone() {
    RendererGone(render_view_host());
  }

  bool is_showing() const {
    return static_cast<TestRenderWidgetHostView*>(render_view_host()->view())->
        is_showing();
  }

  void ClearStates() {
    state_ = NULL;
    deleted_ = NULL;
    delegate_ = NULL;
  }

  void set_delegate(Delegate* delegate) {
    delegate_ = delegate;
  }

 protected:
  virtual RenderViewHost* CreateRenderViewHost() {
    return new TestRenderViewHost(
        SiteInstance::CreateSiteInstance(tab()->profile()),
        this, MSG_ROUTING_NONE, NULL);
  }
 
  virtual void CommandReceived(const std::string& command) {
    command_received_count_++;
  }

 private:
  InterstitialState* state_;
  bool* deleted_;
  int command_received_count_;
  Delegate* delegate_;
};

class TestInterstitialPageStateGuard : public TestInterstitialPage::Delegate {
 public:
  explicit TestInterstitialPageStateGuard(
      TestInterstitialPage* interstitial_page)
      : interstitial_page_(interstitial_page) {
    DCHECK(interstitial_page_);
    interstitial_page_->set_delegate(this);
  }
  ~TestInterstitialPageStateGuard() {
    if (interstitial_page_)
      interstitial_page_->ClearStates();
  }

  virtual void TestInterstitialPageDeleted(TestInterstitialPage* interstitial) {
    DCHECK(interstitial_page_ == interstitial);
    interstitial_page_ = NULL;
  }

 private:
  TestInterstitialPage* interstitial_page_;
};

class WebContentsTest : public testing::Test {
 public:
  WebContentsTest() : contents(NULL) {}

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
    if (contents)
    contents->CloseContents();

    // Make sure that we flush any messages related to WebContents destruction
    // before we destroy the profile.
    MessageLoop::current()->RunAllPending();
  }

  void Navigate(int page_id, const GURL& url) {
    DCHECK(contents);
    ViewHostMsg_FrameNavigate_Params params;
    InitNavigateParams(&params, page_id, url);
    contents->TestDidNavigate(contents->rvh(), params);
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
  contents->controller()->RendererDidNavigate(params, &details);

  contents->UpdateTitle(contents->rvh(), 0, L"    Lots O' Whitespace\n");
  EXPECT_EQ(std::wstring(L"Lots O' Whitespace"), contents->GetTitle());
}

// Test simple same-SiteInstance navigation.
TEST_F(WebContentsTest, SimpleNavigation) {
  TestRenderViewHost* orig_rvh = contents->rvh();
  SiteInstance* instance1 = contents->GetSiteInstance();
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_FALSE(orig_rvh->is_loading);

  // Navigate to URL
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  EXPECT_FALSE(contents->cross_navigation_pending());
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
  EXPECT_FALSE(contents->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents->render_view_host());
  EXPECT_EQ(instance1, orig_rvh->site_instance());
  // Controller's entry should now have the SiteInstance, or else we won't be
  // able to find it later.
  EXPECT_EQ(instance1,
            contents->controller()->GetActiveEntry()->site_instance());
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

  EXPECT_FALSE(contents->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents->render_view_host());

  // Navigate to new site
  const GURL url2("http://www.yahoo.com");
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  EXPECT_TRUE(contents->cross_navigation_pending());
  TestRenderViewHost* pending_rvh = contents->pending_rvh();
  int pending_rvh_delete_count = 0;
  pending_rvh->set_delete_counter(&pending_rvh_delete_count);

  // DidNavigate from the pending page
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 1, url2);
  contents->TestDidNavigate(pending_rvh, params2);
  SiteInstance* instance2 = contents->GetSiteInstance();

  EXPECT_FALSE(contents->cross_navigation_pending());
  EXPECT_EQ(pending_rvh, contents->render_view_host());
  EXPECT_NE(instance1, instance2);
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_EQ(orig_rvh_delete_count, 1);

  // Going back should switch SiteInstances again.  The first SiteInstance is
  // stored in the NavigationEntry, so it should be the same as at the start.
  contents->controller()->GoBack();
  TestRenderViewHost* goback_rvh = contents->pending_rvh();
  EXPECT_TRUE(contents->cross_navigation_pending());

  // DidNavigate from the back action
  contents->TestDidNavigate(goback_rvh, params1);
  EXPECT_FALSE(contents->cross_navigation_pending());
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

  EXPECT_FALSE(contents->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents->render_view_host());

  // Crash the renderer.
  orig_rvh->is_created = false;

  // Navigate to new site.  We should not go into PENDING.
  const GURL url2("http://www.yahoo.com");
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  TestRenderViewHost* new_rvh = contents->rvh();
  EXPECT_FALSE(contents->cross_navigation_pending());
  EXPECT_TRUE(contents->pending_rvh() == NULL);
  EXPECT_NE(orig_rvh, new_rvh);
  EXPECT_EQ(orig_rvh_delete_count, 1);

  // DidNavigate from the new page
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 1, url2);
  contents->TestDidNavigate(new_rvh, params2);
  SiteInstance* instance2 = contents->GetSiteInstance();

  EXPECT_FALSE(contents->cross_navigation_pending());
  EXPECT_EQ(new_rvh, contents->render_view_host());
  EXPECT_NE(instance1, instance2);
  EXPECT_TRUE(contents->pending_rvh() == NULL);
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
  EXPECT_TRUE(contents2->cross_navigation_pending());

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
  // The first RVH in contents2 isn't live yet, so we shortcut the cross site
  // pending.
  TestRenderViewHost* rvh2 = contents2->rvh();
  EXPECT_FALSE(contents2->cross_navigation_pending());
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 2, url2);
  contents2->TestDidNavigate(rvh2, params2);
  SiteInstance* instance2 = contents2->GetSiteInstance();
  EXPECT_NE(instance1, instance2);
  EXPECT_FALSE(contents2->cross_navigation_pending());

  // Simulate a link click in first tab to second site.  Doesn't switch
  // SiteInstances, because we don't intercept WebKit navigations.
  ViewHostMsg_FrameNavigate_Params params3;
  InitNavigateParams(&params3, 2, url2);
  contents->TestDidNavigate(orig_rvh, params3);
  SiteInstance* instance3 = contents->GetSiteInstance();
  EXPECT_EQ(instance1, instance3);
  EXPECT_FALSE(contents->cross_navigation_pending());

  // Navigate to the new site.  Doesn't switch SiteInstancees, because we
  // compare against the current URL, not the SiteInstance's site.
  const GURL url3("http://mail.yahoo.com");
  contents->controller()->LoadURL(url3, GURL(), PageTransition::TYPED);
  EXPECT_FALSE(contents->cross_navigation_pending());
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
  EXPECT_FALSE(contents->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents->render_view_host());

  // Navigate to new site, but simulate an onbeforeunload denial.
  const GURL url2("http://www.yahoo.com");
  orig_rvh->immediate_before_unload = false;
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  orig_rvh->TestOnMsgShouldClose(false);
  EXPECT_FALSE(contents->cross_navigation_pending());
  EXPECT_EQ(orig_rvh, contents->render_view_host());

  // Navigate again, but simulate an onbeforeunload approval.
  contents->controller()->LoadURL(url2, GURL(), PageTransition::TYPED);
  orig_rvh->TestOnMsgShouldClose(true);
  EXPECT_TRUE(contents->cross_navigation_pending());
  TestRenderViewHost* pending_rvh = contents->pending_rvh();

  // We won't hear DidNavigate until the onunload handler has finished running.
  // (No way to simulate that here, but it involves a call from RDH to
  // WebContents::OnCrossSiteResponse.)

  // DidNavigate from the pending page
  ViewHostMsg_FrameNavigate_Params params2;
  InitNavigateParams(&params2, 1, url2);
  contents->TestDidNavigate(pending_rvh, params2);
  SiteInstance* instance2 = contents->GetSiteInstance();
  EXPECT_FALSE(contents->cross_navigation_pending());
  EXPECT_EQ(pending_rvh, contents->render_view_host());
  EXPECT_NE(instance1, instance2);
  EXPECT_TRUE(contents->pending_rvh() == NULL);
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

////////////////////////////////////////////////////////////////////////////////
// Interstitial Tests
////////////////////////////////////////////////////////////////////////////////

// Test navigating to a page (with the navigation initiated from the browser,
// as when a URL is typed in the location bar) that shows an interstitial and
// creates a new navigation entry, then hiding it without proceeding.
TEST_F(WebContentsTest,
       ShowInterstitialFromBrowserWithNewNavigationDontProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  Navigate(1, url1);
  EXPECT_EQ(1, contents->controller()->GetEntryCount());

  // Initiate a browser navigation that will trigger the interstitial
  contents->controller()->LoadURL(GURL("http://www.evil.com"), GURL(),
                                  PageTransition::TYPED);

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::UNDECIDED;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents, true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == interstitial);
  NavigationEntry* entry = contents->controller()->GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->url() == url2);

  // Now don't proceed.
  interstitial->DontProceed();
  EXPECT_TRUE(deleted);
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_FALSE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == NULL);
  entry = contents->controller()->GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->url() == url1);
  EXPECT_EQ(1, contents->controller()->GetEntryCount());
}

// Test navigating to a page (with the navigation initiated from the renderer,
// as when clicking on a link in the page) that shows an interstitial and
// creates a new navigation entry, then hiding it without proceeding.
TEST_F(WebContentsTest,
       ShowInterstitiaFromRendererlWithNewNavigationDontProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  Navigate(1, url1);
  EXPECT_EQ(1, contents->controller()->GetEntryCount());

  // Show an interstitial (no pending entry, the interstitial would have been
  // triggered by clicking on a link).
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::UNDECIDED;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents, true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == interstitial);
  NavigationEntry* entry = contents->controller()->GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->url() == url2);

  // Now don't proceed.
  interstitial->DontProceed();
  EXPECT_TRUE(deleted);
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_FALSE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == NULL);
  entry = contents->controller()->GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->url() == url1);
  EXPECT_EQ(1, contents->controller()->GetEntryCount());
}

// Test navigating to a page that shows an interstitial without creating a new
// navigation entry (this happens when the interstitial is triggered by a
// sub-resource in the page), then hiding it without proceeding.
TEST_F(WebContentsTest, ShowInterstitialNoNewNavigationDontProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  Navigate(1, url1);
  EXPECT_EQ(1, contents->controller()->GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::UNDECIDED;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents, false, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == interstitial);
  NavigationEntry* entry = contents->controller()->GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  // The URL specified to the interstitial should have been ignored.
  EXPECT_TRUE(entry->url() == url1);

  // Now don't proceed.
  interstitial->DontProceed();
  EXPECT_TRUE(deleted);
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
  EXPECT_FALSE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == NULL);
  entry = contents->controller()->GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->url() == url1);
  EXPECT_EQ(1, contents->controller()->GetEntryCount());
}

// Test navigating to a page (with the navigation initiated from the browser,
// as when a URL is typed in the location bar) that shows an interstitial and
// creates a new navigation entry, then proceeding.
TEST_F(WebContentsTest,
       ShowInterstitialFromBrowserNewNavigationProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  Navigate(1, url1);
  EXPECT_EQ(1, contents->controller()->GetEntryCount());

  // Initiate a browser navigation that will trigger the interstitial
  contents->controller()->LoadURL(GURL("http://www.evil.com"), GURL(),
                                  PageTransition::TYPED);

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::UNDECIDED;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents, true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == interstitial);
  NavigationEntry* entry = contents->controller()->GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->url() == url2);

  // Then proceed.
  interstitial->Proceed();
  // The interstitial should show until the new navigation commits.
  ASSERT_FALSE(deleted);
  EXPECT_EQ(TestInterstitialPage::OKED, state);
  EXPECT_TRUE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == interstitial);

  // Simulate the navigation to the page, that's when the interstitial gets
  // hidden.
  GURL url3("http://www.thepage.com");
  Navigate(2, url3);

  EXPECT_TRUE(deleted);
  EXPECT_FALSE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == NULL);
  entry = contents->controller()->GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->url() == url3);

  EXPECT_EQ(2, contents->controller()->GetEntryCount());
}

// Test navigating to a page (with the navigation initiated from the renderer,
// as when clicking on a link in the page) that shows an interstitial and
// creates a new navigation entry, then proceeding.
TEST_F(WebContentsTest,
       ShowInterstitialFromRendererNewNavigationProceed) {
  // Navigate to a page.
  GURL url1("http://www.google.com");
  Navigate(1, url1);
  EXPECT_EQ(1, contents->controller()->GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::UNDECIDED;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents, true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == interstitial);
  NavigationEntry* entry = contents->controller()->GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->url() == url2);

  // Then proceed.
  interstitial->Proceed();
  // The interstitial should show until the new navigation commits.
  ASSERT_FALSE(deleted);
  EXPECT_EQ(TestInterstitialPage::OKED, state);
  EXPECT_TRUE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == interstitial);

  // Simulate the navigation to the page, that's when the interstitial gets
  // hidden.
  GURL url3("http://www.thepage.com");
  Navigate(2, url3);

  EXPECT_TRUE(deleted);
  EXPECT_FALSE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == NULL);
  entry = contents->controller()->GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->url() == url3);

  EXPECT_EQ(2, contents->controller()->GetEntryCount());
}

// Test navigating to a page that shows an interstitial without creating a new
// navigation entry (this happens when the interstitial is triggered by a
// sub-resource in the page), then proceeding.
TEST_F(WebContentsTest, ShowInterstitialNoNewNavigationProceed) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  Navigate(1, url1);
  EXPECT_EQ(1, contents->controller()->GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::UNDECIDED;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents, false, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // The interstitial should not show until its navigation has committed.
  EXPECT_FALSE(interstitial->is_showing());
  EXPECT_FALSE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == NULL);
  // Let's commit the interstitial navigation.
  interstitial->TestDidNavigate(1, url2);
  EXPECT_TRUE(interstitial->is_showing());
  EXPECT_TRUE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == interstitial);
  NavigationEntry* entry = contents->controller()->GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  // The URL specified to the interstitial should have been ignored.
  EXPECT_TRUE(entry->url() == url1);

  // Then proceed.
  interstitial->Proceed();
  // Since this is not a new navigation, the previous page is dismissed right
  // away and shows the original page.
  EXPECT_TRUE(deleted);
  EXPECT_EQ(TestInterstitialPage::OKED, state);
  EXPECT_FALSE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == NULL);
  entry = contents->controller()->GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->url() == url1);

  EXPECT_EQ(1, contents->controller()->GetEntryCount());
}

// Test navigating to a page that shows an interstitial, then navigating away.
TEST_F(WebContentsTest, ShowInterstitialThenNavigate) {
  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::UNDECIDED;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents, true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url);

  // While interstitial showing, navigate to a new URL.
  const GURL url2("http://www.yahoo.com");
  Navigate(1, url2);

  EXPECT_TRUE(deleted);
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
}

// Test navigating to a page that shows an interstitial, then close the tab.
TEST_F(WebContentsTest, ShowInterstitialThenCloseTab) {
  // Show interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::UNDECIDED;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents, true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url);

  // Now close the tab.
  contents->CloseContents();
  contents = NULL;  // So we don't detroy it again on TearDown.
  EXPECT_TRUE(deleted);
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
}

// Test that after Proceed is called and an interstitial is still shown, no more
// commands get executed.
TEST_F(WebContentsTest, ShowInterstitialProceedMultipleCommands) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL url1("http://www.google.com");
  Navigate(1, url1);
  EXPECT_EQ(1, contents->controller()->GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::UNDECIDED;
  bool deleted = false;
  GURL url2("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents, true, url2, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url2);

  // Run a command.
  EXPECT_EQ(0, interstitial->command_received_count());
  interstitial->TestDomOperationResponse("toto");
  EXPECT_EQ(1, interstitial->command_received_count());

  // Then proceed.
  interstitial->Proceed();
  ASSERT_FALSE(deleted);

  // While the navigation to the new page is pending, send other commands, they
  // should be ignored.
  interstitial->TestDomOperationResponse("hello");
  interstitial->TestDomOperationResponse("hi");
  EXPECT_EQ(1, interstitial->command_received_count());
}

// Test showing an interstitial while another interstitial is already showing.
TEST_F(WebContentsTest, ShowInterstitialOnInterstitial) {
  // Navigate to a page so we have a navigation entry in the controller.
  GURL start_url("http://www.google.com");
  Navigate(1, start_url);
  EXPECT_EQ(1, contents->controller()->GetEntryCount());

  // Show an interstitial.
  TestInterstitialPage::InterstitialState state1 =
      TestInterstitialPage::UNDECIDED;
  bool deleted1 = false;
  GURL url1("http://interstitial1");
  TestInterstitialPage* interstitial1 =
      new TestInterstitialPage(contents, true, url1, &state1, &deleted1);
  TestInterstitialPageStateGuard state_guard1(interstitial1);
  interstitial1->Show();
  interstitial1->TestDidNavigate(1, url1);

  // Now show another interstitial.
  TestInterstitialPage::InterstitialState state2 =
      TestInterstitialPage::UNDECIDED;
  bool deleted2 = false;
  GURL url2("http://interstitial2");
  TestInterstitialPage* interstitial2 =
      new TestInterstitialPage(contents, true, url2, &state2, &deleted2);
  TestInterstitialPageStateGuard state_guard2(interstitial2);
  interstitial2->Show();
  interstitial2->TestDidNavigate(1, url2);

  // Showing interstitial2 should have caused interstitial1 to go away.
  EXPECT_TRUE(deleted1);
  EXPECT_EQ(TestInterstitialPage::CANCELED, state1);
  
  // Let's make sure interstitial2 is working as intended.
  ASSERT_FALSE(deleted2);
  EXPECT_EQ(TestInterstitialPage::UNDECIDED, state2);  
  interstitial2->Proceed();
  GURL landing_url("http://www.thepage.com");
  Navigate(2, landing_url);

  EXPECT_TRUE(deleted2);
  EXPECT_FALSE(contents->showing_interstitial_page());
  EXPECT_TRUE(contents->interstitial_page() == NULL);
  NavigationEntry* entry = contents->controller()->GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->url() == landing_url);
  EXPECT_EQ(2, contents->controller()->GetEntryCount());
}

// Test that navigating away from an interstitial while it's loading cause it
// not to show.
TEST_F(WebContentsTest, NavigateBeforeInterstitialShows) {
  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::UNDECIDED;
  bool deleted = false;
  GURL interstitial_url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents, true, interstitial_url,
                               &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();

  // Let's simulate a navigation initiated from the browser before the
  // interstitial finishes loading.
  const GURL url("http://www.google.com");
  contents->controller()->LoadURL(url, GURL(), PageTransition::TYPED);
  ASSERT_FALSE(deleted);
  EXPECT_FALSE(interstitial->is_showing());

  // Now let's make the interstitial navigation commit.
  interstitial->TestDidNavigate(1, interstitial_url);

  // After it loaded the interstitial should be gone.
  EXPECT_TRUE(deleted);
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
}

// Test showing an interstitial and have its renderer crash.
TEST_F(WebContentsTest, InterstitialCrasher) {
  // Show an interstitial.
  TestInterstitialPage::InterstitialState state =
      TestInterstitialPage::UNDECIDED;
  bool deleted = false;
  GURL url("http://interstitial");
  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents, true, url, &state, &deleted);
  TestInterstitialPageStateGuard state_guard(interstitial);
  interstitial->Show();
  // Simulate a renderer crash before the interstitial is shown.
  interstitial->TestRendererGone();
  // The interstitial should have been dismissed.
  EXPECT_TRUE(deleted);
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);

  // Now try again but this time crash the intersitial after it was shown.
  interstitial =
      new TestInterstitialPage(contents, true, url, &state, &deleted);
  interstitial->Show();
  interstitial->TestDidNavigate(1, url);
  // Simulate a renderer crash.
  interstitial->TestRendererGone();
  // The interstitial should have been dismissed.
  EXPECT_TRUE(deleted);
  EXPECT_EQ(TestInterstitialPage::CANCELED, state);
}
