// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_type.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/session_service.h"
#include "chrome/browser/session_service_test_helper.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/browser/tab_contents_factory.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_types.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/test/test_notification_tracker.h"
#include "chrome/test/testing_profile.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// TODO(darin): come up with a better way to define these integers
// TODO(acw): we should have a real dynamic factory for content types.
// That way we could have several implementation of
// TabContents::CreateWithType(). Once this is done we'll be able to
// have a unit test for NavigationController::Clone()
const TabContentsType kTestContentsType1 =
    static_cast<TabContentsType>(TAB_CONTENTS_NUM_TYPES + 1);
const TabContentsType kTestContentsType2 =
    static_cast<TabContentsType>(TAB_CONTENTS_NUM_TYPES + 2);

// Tests can set this to set the site instance for all the test contents. This
// refcounted pointer will NOT be derefed on cleanup (the tests do this
// themselves).
static SiteInstance* site_instance;

// TestContents ----------------------------------------------------------------

class TestContents : public TabContents {
 public:
  BEGIN_MSG_MAP(TestContents)
  END_MSG_MAP()

  TestContents(TabContentsType type) : TabContents(type) {
  }

  // Overridden from TabContents so we can provide a non-NULL site instance in
  // some cases. To use, the test will have to set the site_instance_ member
  // variable to some site instance it creates.
  virtual SiteInstance* GetSiteInstance() const {
    return site_instance;
  }

  // Just record the navigation so it can be checked by the test case. We don't
  // want the normal behavior of TabContents just saying it committed since we
  // want to behave more like the renderer and call RendererDidNavigate.
  virtual bool NavigateToPendingEntry(bool reload) {
    return true;
  }

  // Sets up a call to RendererDidNavigate pretending to be a main frame
  // navigation to the given URL.
  void CompleteNavigationAsRenderer(int page_id, const GURL& url) {
    ViewHostMsg_FrameNavigate_Params params;
    params.page_id = page_id;
    params.url = url;
    params.transition = PageTransition::LINK;
    params.should_update_history = false;
    params.gesture = NavigationGestureUser;
    params.is_post = false;

    NavigationController::LoadCommittedDetails details;
    controller()->RendererDidNavigate(params, false, &details);
  }
};

class TestContentsFactory : public TabContentsFactory {
 public:
  TestContentsFactory(TabContentsType type, const char* scheme)
      : type_(type),
        scheme_(scheme) {
  }

  virtual TabContents* CreateInstance() {
    return new TestContents(type_);
  }

  virtual bool CanHandleURL(const GURL& url) {
    return url.SchemeIs(scheme_);
  }

 private:
  TabContentsType type_;
  const char* scheme_;
};

TestContentsFactory factory1(kTestContentsType1, "test1");
TestContentsFactory factory2(kTestContentsType2, "test2");

// NavigationControllerTest ----------------------------------------------------

class NavigationControllerTest : public testing::Test,
                                 public TabContentsDelegate {
 public:
  NavigationControllerTest() : contents(NULL), profile(NULL) {
  }

  ~NavigationControllerTest() {
    delete profile;
  }

  // testing::Test methods:

  virtual void SetUp() {
    TabContents::RegisterFactory(kTestContentsType1, &factory1);
    TabContents::RegisterFactory(kTestContentsType2, &factory2);

    if (!profile)
      profile = new TestingProfile();

    contents = new TestContents(kTestContentsType1);
    contents->set_delegate(this);
    contents->CreateView(::GetDesktopWindow(), gfx::Rect());
    contents->SetupController(profile);
  }

  virtual void TearDown() {
    site_instance = NULL;

    // Make sure contents is valid. NavigationControllerHistoryTest ends up
    // resetting this before TearDown is invoked.
    if (contents)
      ClearContents();
  }


  void ClearContents() {
    contents->set_delegate(NULL);
    contents->CloseContents();
    contents = NULL;

    TabContents::RegisterFactory(kTestContentsType1, NULL);
    TabContents::RegisterFactory(kTestContentsType2, NULL);
  }

  // TabContentsDelegate methods (only care about ReplaceContents):
  virtual void OpenURLFromTab(TabContents*,
                              const GURL&,
                              WindowOpenDisposition,
                              PageTransition::Type) {}
  virtual void NavigationStateChanged(const TabContents*,
                                      unsigned flags) {}
  virtual void ReplaceContents(TabContents* source,
                               TabContents* new_contents) {
    contents->set_delegate(NULL);
    contents = static_cast<TestContents*>(new_contents);
    contents->set_delegate(this);
  }
  virtual void AddNewContents(TabContents*,
                              TabContents*,
                              WindowOpenDisposition,
                              const gfx::Rect&,
                              bool user_gesture) {}
  virtual void ActivateContents(TabContents*) {}
  virtual void LoadingStateChanged(TabContents*) {}
  virtual void NavigateToPage(TabContents*, const GURL&,
                              PageTransition::Type) {}
  virtual void CloseContents(TabContents*) {}
  virtual void MoveContents(TabContents*, const gfx::Rect&) {}
  virtual bool IsPopup(TabContents*) { return false; }
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
  virtual void URLStarredChanged(TabContents* source, bool starred) {}
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {};

  TestContents* contents;

  Profile* profile;
 
 private:
  MessageLoopForUI message_loop_;
};

// NavigationControllerHistoryTest ---------------------------------------------

class NavigationControllerHistoryTest : public NavigationControllerTest {
 public:
  NavigationControllerHistoryTest()
      : profile_manager_(NULL),
        url0("test1:foo1"),
        url1("test1:foo1"),
        url2("test1:foo1") {
  }

  virtual ~NavigationControllerHistoryTest() {
    // Prevent our base class from deleting the profile since profile's
    // lifetime is managed by profile_manager_.
    profile = NULL;
    STLDeleteElements(&windows_);
  }

  virtual void SetUp() {
    // Calculate the path for a scratch profile, and make sure it's empty.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    file_util::AppendToPath(&test_dir_, L"NavigationControllerTest");
    profile_path_ = test_dir_;
    file_util::AppendToPath(&profile_path_, L"New Profile");
    file_util::Delete(test_dir_, true);
    CreateDirectory(test_dir_.c_str(), NULL);

    // Create a profile.
    profile_manager_ = new ProfileManager();
    profile = ProfileManager::CreateProfile(profile_path_,
        L"New Profile", L"new-profile", L"");
    ASSERT_TRUE(profile);
    profile_manager_->AddProfile(profile);

    // Do the super thing. Notice that the above code sets profile, profile is
    // used in NavigationControllerTest::SetUp(), hence it now.
    NavigationControllerTest::SetUp();

    // Force the session service to be created.
    SessionService* service = profile->GetSessionService();
    service->SetWindowType(window_id, BrowserType::TABBED_BROWSER);
    service->SetWindowBounds(window_id, gfx::Rect(0, 1, 2, 3), false);
    service->SetTabIndexInWindow(window_id,
                                 contents->controller()->session_id(), 0);
    contents->controller()->SetWindowID(window_id);
  }

  virtual void TearDown() {
    NavigationControllerTest::TearDown();

    helper_.set_service(NULL);

    // Make sure we wait for history to shut down before continuing. The task
    // we add will cause our message loop to quit once it is destroyed.
    HistoryService* history =
        profile->GetHistoryService(Profile::IMPLICIT_ACCESS);
    history->SetOnBackendDestroyTask(new MessageLoop::QuitTask);
    delete profile_manager_;
    MessageLoop::current()->Run();

    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // Deletes the current profile manager and creates a new one. Indirectly this
  // shuts down the history database and reopens it.
  void ReopenDatabase() {
    ClearContents();
    helper_.set_service(NULL);
    delete profile_manager_;
    profile_manager_ = new ProfileManager();
    profile_manager_->AddProfileByPath(profile_path_);
    profile = profile_manager_->GetProfileByPath(profile_path_);
    helper_.set_service(profile->GetSessionService());
  }

  void GetLastSession() {
    Profile* profile = contents->profile();
    profile->GetSessionService()->TabClosed(
        contents->controller()->window_id(),
        contents->controller()->session_id());

    ReopenDatabase();
    Time close_time;

    helper_.ReadWindows(&windows_);
  }

  CancelableRequestConsumer consumer;

  // URLs for testing.
  const GURL url0;
  const GURL url1;
  const GURL url2;

  std::vector<SessionWindow*> windows_;

  SessionID window_id;

  SessionServiceTestHelper helper_;

 private:
  ProfileManager* profile_manager_;
  std::wstring test_dir_;
  std::wstring profile_path_;
};

void RegisterForAllNavNotifications(TestNotificationTracker* tracker,
                                    NavigationController* controller) {
  tracker->ListenFor(NOTIFY_NAV_ENTRY_COMMITTED,
                     Source<NavigationController>(controller));
  tracker->ListenFor(NOTIFY_NAV_LIST_PRUNED,
                     Source<NavigationController>(controller));
  tracker->ListenFor(NOTIFY_NAV_ENTRY_CHANGED,
                     Source<NavigationController>(controller));
}

}  // namespace

// -----------------------------------------------------------------------------

TEST_F(NavigationControllerTest, Defaults) {
  EXPECT_TRUE(contents->is_active());
  EXPECT_TRUE(contents->controller());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->GetLastCommittedEntry());
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), -1);
  EXPECT_EQ(contents->controller()->GetEntryCount(), 0);
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
}

TEST_F(NavigationControllerTest, LoadURL) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");
  const GURL url2("test1:foo2");

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  // Creating a pending notification should not have issued any of the
  // notifications we're listening for.
  EXPECT_EQ(0, notifications.size());

  // The load should now be pending.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 0);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), -1);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_FALSE(contents->controller()->GetLastCommittedEntry());
  EXPECT_TRUE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
  EXPECT_EQ(contents->GetMaxPageID(), -1);

  // We should have gotten no notifications from the preceeding checks.
  EXPECT_EQ(0, notifications.size());

  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // The load should now be committed.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 1);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
  EXPECT_EQ(contents->GetMaxPageID(), 0);

  // Load another...
  contents->controller()->LoadURL(url2, PageTransition::TYPED);

  // The load should now be pending.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 1);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_TRUE(contents->controller()->GetPendingEntry());
  // TODO(darin): maybe this should really be true?
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
  EXPECT_EQ(contents->GetMaxPageID(), 0);

  contents->CompleteNavigationAsRenderer(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // The load should now be committed.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_TRUE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
  EXPECT_EQ(contents->GetMaxPageID(), 1);
}

// Tests what happens when the same page is loaded again.  Should not create a
// new session history entry. This is what happens when you press enter in the
// URL bar to reload: a pending entry is created and then it is discarded when
// the load commits (because WebCore didn't actually make a new entry).
TEST_F(NavigationControllerTest, LoadURL_SamePage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  EXPECT_EQ(0, notifications.size());
  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  EXPECT_EQ(0, notifications.size());
  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // We should not have produced a new session history entry.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 1);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
}

// Tests loading a URL but discarding it before the load commits.
TEST_F(NavigationControllerTest, LoadURL_Discarded) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");
  const GURL url2("test1:foo2");

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  EXPECT_EQ(0, notifications.size());
  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  contents->controller()->LoadURL(url2, PageTransition::TYPED);
  contents->controller()->DiscardPendingEntry();
  EXPECT_EQ(0, notifications.size());

  // Should not have produced a new session history entry.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 1);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
}

// Tests navigations that come in unrequested. This happens when the user
// navigates from the web page, and here we test that there is no pending entry.
TEST_F(NavigationControllerTest, LoadURL_NoPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  // First make an existing committed entry.
  const GURL kExistingURL1("test1:eh");
  contents->controller()->LoadURL(kExistingURL1, PageTransition::TYPED);
  contents->CompleteNavigationAsRenderer(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));
	
  // Do a new navigation without making a pending one.
  const GURL kNewURL("test1:see");
  contents->CompleteNavigationAsRenderer(99, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(-1, contents->controller()->GetPendingEntryIndex());
  EXPECT_EQ(1, contents->controller()->GetLastCommittedEntryIndex());
  EXPECT_EQ(kNewURL, contents->controller()->GetActiveEntry()->url());
}

// Tests navigating to a new URL when there is a new pending navigation that is
// not the one that just loaded. This will happen if the user types in a URL to
// somewhere slow, and then navigates the current page before the typed URL
// commits.
TEST_F(NavigationControllerTest, LoadURL_NewPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  // First make an existing committed entry.
  const GURL kExistingURL1("test1:eh");
  contents->controller()->LoadURL(kExistingURL1, PageTransition::TYPED);
  contents->CompleteNavigationAsRenderer(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // Make a pending entry to somewhere new.
  const GURL kExistingURL2("test1:bee");
  contents->controller()->LoadURL(kExistingURL2, PageTransition::TYPED);
  EXPECT_EQ(0, notifications.size());

  // Before that commits, do a new navigation.
  const GURL kNewURL("test1:see");
  contents->CompleteNavigationAsRenderer(3, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(-1, contents->controller()->GetPendingEntryIndex());
  EXPECT_EQ(1, contents->controller()->GetLastCommittedEntryIndex());
  EXPECT_EQ(kNewURL, contents->controller()->GetActiveEntry()->url());
}

// Tests navigating to a new URL when there is a pending back/forward
// navigation. This will happen if the user hits back, but before that commits,
// they navigate somewhere new.
TEST_F(NavigationControllerTest, LoadURL_ExistingPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  // First make some history.
  const GURL kExistingURL1("test1:eh");
  contents->controller()->LoadURL(kExistingURL1, PageTransition::TYPED);
  contents->CompleteNavigationAsRenderer(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  const GURL kExistingURL2("test1:bee");
  contents->controller()->LoadURL(kExistingURL2, PageTransition::TYPED);
  contents->CompleteNavigationAsRenderer(1, kExistingURL2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // Now make a pending back/forward navigation. The zeroth entry should be
  // pending.
  contents->controller()->GoBack();
  EXPECT_EQ(0, notifications.size());
  EXPECT_EQ(0, contents->controller()->GetPendingEntryIndex());
  EXPECT_EQ(1, contents->controller()->GetLastCommittedEntryIndex());

  // Before that commits, do a new navigation.
  const GURL kNewURL("test1:see");
  NavigationController::LoadCommittedDetails details;
  contents->CompleteNavigationAsRenderer(3, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(-1, contents->controller()->GetPendingEntryIndex());
  EXPECT_EQ(2, contents->controller()->GetLastCommittedEntryIndex());
  EXPECT_EQ(kNewURL, contents->controller()->GetActiveEntry()->url());
}

TEST_F(NavigationControllerTest, Reload) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  EXPECT_EQ(0, notifications.size());
  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  contents->controller()->Reload();
  EXPECT_EQ(0, notifications.size());

  // The reload is pending.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 1);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), 0);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_TRUE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());

  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // Now the reload is committed.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 1);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
}

// Tests what happens when a reload navigation produces a new page.
TEST_F(NavigationControllerTest, Reload_GeneratesNewPage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");
  const GURL url2("test1:foo2");

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  contents->controller()->Reload();
  EXPECT_EQ(0, notifications.size());

  contents->CompleteNavigationAsRenderer(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // Now the reload is committed.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_TRUE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
}

// Tests what happens when we navigate back successfully
TEST_F(NavigationControllerTest, Back) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");
  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  const GURL url2("test1:foo2");
  contents->CompleteNavigationAsRenderer(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  contents->controller()->GoBack();
  EXPECT_EQ(0, notifications.size());

  // We should now have a pending navigation to go back.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), 0);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_TRUE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_TRUE(contents->controller()->CanGoForward());

  contents->CompleteNavigationAsRenderer(0, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // The back navigation completed successfully.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_TRUE(contents->controller()->CanGoForward());
}

// Tests what happens when a back navigation produces a new page.
TEST_F(NavigationControllerTest, Back_GeneratesNewPage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");
  const GURL url2("test1:foo2");
  const GURL url3("test1:foo3");

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  contents->controller()->LoadURL(url2, PageTransition::TYPED);
  contents->CompleteNavigationAsRenderer(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  contents->controller()->GoBack();
  EXPECT_EQ(0, notifications.size());

  // We should now have a pending navigation to go back.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), 0);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_TRUE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_TRUE(contents->controller()->CanGoForward());

  contents->CompleteNavigationAsRenderer(2, url3);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // The back navigation resulted in a completely new navigation.
  // TODO(darin): perhaps this behavior will be confusing to users?
  EXPECT_EQ(contents->controller()->GetEntryCount(), 3);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 2);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_TRUE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
}

// Receives a back message when there is a new pending navigation entry.
TEST_F(NavigationControllerTest, Back_NewPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL kUrl1("test1:foo1");
  const GURL kUrl2("test1:foo2");
  const GURL kUrl3("test1:foo3");

  // First navigate two places so we have some back history.
  contents->CompleteNavigationAsRenderer(0, kUrl1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  //contents->controller()->LoadURL(kUrl2, PageTransition::TYPED);
  contents->CompleteNavigationAsRenderer(1, kUrl2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // Now start a new pending navigation and go back before it commits.
  contents->controller()->LoadURL(kUrl3, PageTransition::TYPED);
  EXPECT_EQ(-1, contents->controller()->GetPendingEntryIndex());
  EXPECT_EQ(kUrl3, contents->controller()->GetPendingEntry()->url());
  contents->controller()->GoBack();

  // The pending navigation should now be the "back" item and the new one
  // should be gone.
  EXPECT_EQ(0, contents->controller()->GetPendingEntryIndex());
  EXPECT_EQ(kUrl1, contents->controller()->GetPendingEntry()->url());
}

// Receives a back message when there is a different renavigation already
// pending.
TEST_F(NavigationControllerTest, Back_OtherBackPending) {
  const GURL kUrl1("test1:foo1");
  const GURL kUrl2("test1:foo2");
  const GURL kUrl3("test1:foo3");

  // First navigate three places so we have some back history.
  contents->CompleteNavigationAsRenderer(0, kUrl1);
  contents->CompleteNavigationAsRenderer(1, kUrl2);
  contents->CompleteNavigationAsRenderer(2, kUrl3);

  // With nothing pending, say we get a navigation to the second entry.
  contents->CompleteNavigationAsRenderer(1, kUrl2);

  // That second URL should be the last committed and it should have gotten the
  // new title.
  EXPECT_EQ(kUrl2, contents->controller()->GetEntryWithPageID(
      kTestContentsType1, NULL, 1)->url());
  EXPECT_EQ(1, contents->controller()->GetLastCommittedEntryIndex());
  EXPECT_EQ(-1, contents->controller()->GetPendingEntryIndex());

  // Now go forward to the last item again and say it was committed.
  contents->controller()->GoForward();
  contents->CompleteNavigationAsRenderer(2, kUrl3);

  // Now start going back one to the second page. It will be pending.
  contents->controller()->GoBack();
  EXPECT_EQ(1, contents->controller()->GetPendingEntryIndex());
  EXPECT_EQ(2, contents->controller()->GetLastCommittedEntryIndex());

  // Not synthesize a totally new back event to the first page. This will not
  // match the pending one.
  contents->CompleteNavigationAsRenderer(0, kUrl1);
  
  // The navigation should not have affected the pending entry.
  EXPECT_EQ(1, contents->controller()->GetPendingEntryIndex());

  // But the navigated entry should be the last committed.
  EXPECT_EQ(0, contents->controller()->GetLastCommittedEntryIndex());
  EXPECT_EQ(kUrl1, contents->controller()->GetLastCommittedEntry()->url());
}

// Tests what happens when we navigate forward successfully.
TEST_F(NavigationControllerTest, Forward) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");
  const GURL url2("test1:foo2");

  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  contents->CompleteNavigationAsRenderer(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  contents->controller()->GoBack();
  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  contents->controller()->GoForward();

  // We should now have a pending navigation to go forward.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), 1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_TRUE(contents->controller()->GetPendingEntry());
  EXPECT_TRUE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());

  contents->CompleteNavigationAsRenderer(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // The forward navigation completed successfully.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_TRUE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
}

// Tests what happens when a forward navigation produces a new page.
TEST_F(NavigationControllerTest, Forward_GeneratesNewPage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");
  const GURL url2("test1:foo2");
  const GURL url3("test1:foo3");

  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));
  contents->CompleteNavigationAsRenderer(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  contents->controller()->GoBack();
  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  contents->controller()->GoForward();
  EXPECT_EQ(0, notifications.size());

  // Should now have a pending navigation to go forward.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), 1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_TRUE(contents->controller()->GetPendingEntry());
  EXPECT_TRUE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());

  contents->CompleteNavigationAsRenderer(2, url3);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_LIST_PRUNED,
                                           NOTIFY_NAV_ENTRY_COMMITTED));

  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_TRUE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
}

// Tests navigation via link click within a subframe. A new navigation entry
// should be created.
TEST_F(NavigationControllerTest, NewSubframe) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");
  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  const GURL url2("test1:foo2");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url2;
  params.transition = PageTransition::MANUAL_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;

  NavigationController::LoadCommittedDetails details;
  EXPECT_TRUE(contents->controller()->RendererDidNavigate(params, false,
                                                          &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(url1, details.previous_url);
  EXPECT_FALSE(details.is_auto);
  EXPECT_FALSE(details.is_in_page);
  EXPECT_FALSE(details.is_main_frame);

  // The new entry should be appended.
  EXPECT_EQ(2, contents->controller()->GetEntryCount());

  // New entry should refer to the new page, but the old URL (entries only
  // reflect the toplevel URL).
  EXPECT_EQ(url1, details.entry->url());
  EXPECT_EQ(params.page_id, details.entry->page_id());
}

// Auto subframes are ones the page loads automatically like ads. They should
// not create new navigation entries.
TEST_F(NavigationControllerTest, AutoSubframe) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");
  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  const GURL url2("test1:foo2");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url2;
  params.transition = PageTransition::AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;

  // Navigating should do nothing.
  NavigationController::LoadCommittedDetails details;
  EXPECT_FALSE(contents->controller()->RendererDidNavigate(params, false,
                                                           &details));
  EXPECT_EQ(0, notifications.size());

  // There should still be only one entry.
  EXPECT_EQ(1, contents->controller()->GetEntryCount());
}

// Tests navigation and then going back to a subframe navigation.
TEST_F(NavigationControllerTest, BackSubframe) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  // Main page.
  const GURL url1("test1:foo1");
  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // First manual subframe navigation.
  const GURL url2("test1:foo2");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url2;
  params.transition = PageTransition::MANUAL_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;

  // This should generate a new entry.
  NavigationController::LoadCommittedDetails details;
  EXPECT_TRUE(contents->controller()->RendererDidNavigate(params, false,
                                                          &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(2, contents->controller()->GetEntryCount());

  // Second manual subframe navigation should also make a new entry.
  const GURL url3("test1:foo3");
  params.page_id = 2;
  params.url = url3;
  EXPECT_TRUE(contents->controller()->RendererDidNavigate(params, false,
                                                          &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(3, contents->controller()->GetEntryCount());
  EXPECT_EQ(2, contents->controller()->GetCurrentEntryIndex());

  // Go back one.
  contents->controller()->GoBack();
  params.url = url2;
  params.page_id = 1;
  EXPECT_TRUE(contents->controller()->RendererDidNavigate(params, false,
                                                          &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(3, contents->controller()->GetEntryCount());
  EXPECT_EQ(1, contents->controller()->GetCurrentEntryIndex());

  // Go back one more.
  contents->controller()->GoBack();
  params.url = url1;
  params.page_id = 0;
  EXPECT_TRUE(contents->controller()->RendererDidNavigate(params, false,
                                                          &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(3, contents->controller()->GetEntryCount());
  EXPECT_EQ(0, contents->controller()->GetCurrentEntryIndex());
}

TEST_F(NavigationControllerTest, LinkClick) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");
  const GURL url2("test1:foo2");

  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  contents->CompleteNavigationAsRenderer(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // Should not have produced a new session history entry.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_TRUE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
}

TEST_F(NavigationControllerTest, InPage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  // Main page. Note that we need "://" so this URL is treated as "standard"
  // which are the only ones that can have a ref.
  const GURL url1("test1://foo");
  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // First navigation.
  const GURL url2("test1://foo#a");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url2;
  params.transition = PageTransition::LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;

  // This should generate a new entry.
  NavigationController::LoadCommittedDetails details;
  EXPECT_TRUE(contents->controller()->RendererDidNavigate(params, false,
                                                          &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(2, contents->controller()->GetEntryCount());

  // Go back one.
  ViewHostMsg_FrameNavigate_Params back_params(params);
  contents->controller()->GoBack();
  back_params.url = url1;
  back_params.page_id = 0;
  EXPECT_TRUE(contents->controller()->RendererDidNavigate(back_params, false,
                                                          &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(2, contents->controller()->GetEntryCount());
  EXPECT_EQ(0, contents->controller()->GetCurrentEntryIndex());
  EXPECT_EQ(back_params.url, contents->controller()->GetActiveEntry()->url());

  // Go forward
  ViewHostMsg_FrameNavigate_Params forward_params(params);
  contents->controller()->GoForward();
  forward_params.url = url2;
  forward_params.page_id = 1;
  EXPECT_TRUE(contents->controller()->RendererDidNavigate(forward_params, false,
                                                          &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(2, contents->controller()->GetEntryCount());
  EXPECT_EQ(1, contents->controller()->GetCurrentEntryIndex());
  EXPECT_EQ(forward_params.url,
            contents->controller()->GetActiveEntry()->url());

  // Now go back and forward again. This is to work around a bug where we would
  // compare the incoming URL with the last committed entry rather than the
  // one identified by an existing page ID. This would result in the second URL
  // losing the reference fragment when you navigate away from it and then back.
  contents->controller()->GoBack();
  EXPECT_TRUE(contents->controller()->RendererDidNavigate(back_params, false,
                                                          &details));
  contents->controller()->GoForward();
  EXPECT_TRUE(contents->controller()->RendererDidNavigate(forward_params, false,
                                                          &details));
  EXPECT_EQ(forward_params.url,
            contents->controller()->GetActiveEntry()->url());
}

TEST_F(NavigationControllerTest, SwitchTypes) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo");
  const GURL url2("test2:foo");

  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  TestContents* initial_contents = contents;
  contents->controller()->LoadURL(url2, PageTransition::TYPED);

  // The tab contents should have been replaced
  ASSERT_TRUE(initial_contents != contents);

  contents->CompleteNavigationAsRenderer(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  // A second navigation entry should have been committed even though the
  // PageIDs are the same.  PageIDs are scoped to the tab contents type.
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_TRUE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());

  // Navigate back...
  contents->controller()->GoBack();
  ASSERT_TRUE(initial_contents == contents);  // switched again!
  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_TRUE(contents->controller()->CanGoForward());

  // There may be TabContentsCollector tasks pending, so flush them from queue.
  MessageLoop::current()->RunAllPending();
}

// Tests what happens when we begin to navigate to a new contents type, but
// then that navigation gets discarded instead.
TEST_F(NavigationControllerTest, SwitchTypes_Discard) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo");
  const GURL url2("test2:foo");

  contents->CompleteNavigationAsRenderer(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFY_NAV_ENTRY_COMMITTED));

  TestContents* initial_contents = contents;

  contents->controller()->LoadURL(url2, PageTransition::TYPED);
  EXPECT_EQ(0, notifications.size());

  // The tab contents should have been replaced
  ASSERT_TRUE(initial_contents != contents);

  contents->controller()->DiscardPendingEntry();
  EXPECT_EQ(0, notifications.size());

  // The tab contents should have been replaced back
  ASSERT_TRUE(initial_contents == contents);

  EXPECT_EQ(contents->controller()->GetEntryCount(), 1);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());

  // There may be TabContentsCollector tasks pending, so flush them from queue.
  MessageLoop::current()->RunAllPending();
}

// Tests that TabContentsTypes that are not in use are deleted (via a
// TabContentsCollector task).  Prevents regression of bug 1296773.
TEST_F(NavigationControllerTest, SwitchTypesCleanup) {
  const GURL url1("test1:foo");
  const GURL url2("test2:foo");
  const GURL url3("test2:bar");

  // Note that we need the LoadURL calls so that pending entries and the
  // different tab contents types are created. "Renderer" navigations won't
  // actually cross tab contents boundaries without these.
  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  contents->CompleteNavigationAsRenderer(0, url1);
  contents->controller()->LoadURL(url2, PageTransition::TYPED);
  contents->CompleteNavigationAsRenderer(1, url2);
  contents->controller()->LoadURL(url3, PageTransition::TYPED);
  contents->CompleteNavigationAsRenderer(2, url3);

  // Navigate back to the start.
  contents->controller()->GoToIndex(0);
  contents->CompleteNavigationAsRenderer(0, url1);

  // Now jump to the end.
  contents->controller()->GoToIndex(2);
  contents->CompleteNavigationAsRenderer(2, url3);

  // There may be TabContentsCollector tasks pending, so flush them from queue.
  MessageLoop::current()->RunAllPending();

  // Now that the tasks have been flushed, the first tab type should be gone.
  ASSERT_TRUE(
      contents->controller()->GetTabContents(kTestContentsType1) == NULL);
  ASSERT_EQ(contents, 
      contents->controller()->GetTabContents(kTestContentsType2));
}

namespace {

// NotificationObserver implementation used in verifying we've received the
// NOTIFY_NAV_LIST_PRUNED method.
class PrunedListener : public NotificationObserver {
 public:
  explicit PrunedListener(NavigationController* controller)
      : notification_count_(0) {
    registrar_.Add(this, NOTIFY_NAV_LIST_PRUNED,
                   Source<NavigationController>(controller));
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NOTIFY_NAV_LIST_PRUNED) {
      notification_count_++;
      details_ = *(Details<NavigationController::PrunedDetails>(details).ptr());
    }
  }

  // Number of times NOTIFY_NAV_LIST_PRUNED has been observed.
  int notification_count_;

  // Details from the last NOTIFY_NAV_LIST_PRUNED.
  NavigationController::PrunedDetails details_;

 private:
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrunedListener);
};

}

// Tests that we limit the number of navigation entries created correctly.
TEST_F(NavigationControllerTest, EnforceMaxNavigationCount) {
  size_t original_count = NavigationController::max_entry_count();
  const size_t kMaxEntryCount = 5;

  NavigationController::set_max_entry_count(kMaxEntryCount);

  int url_index;
  char buffer[128];
  // Load up to the max count, all entries should be there.
  for (url_index = 0; url_index < kMaxEntryCount; url_index++) {
    SNPrintF(buffer, 128, "test1://www.a.com/%d", url_index);
    GURL url(buffer);
    contents->controller()->LoadURL(url, PageTransition::TYPED);
    contents->CompleteNavigationAsRenderer(url_index, url);
  }

  EXPECT_EQ(contents->controller()->GetEntryCount(), kMaxEntryCount);

  // Created a PrunedListener to observe prune notifications.
  PrunedListener listener(contents->controller());

  // Navigate some more.
  SNPrintF(buffer, 128, "test1://www.a.com/%d", url_index);
  GURL url(buffer);
  contents->controller()->LoadURL(url, PageTransition::TYPED);
  contents->CompleteNavigationAsRenderer(url_index, url);
  url_index++;

  // We should have got a pruned navigation.
  EXPECT_EQ(1, listener.notification_count_);
  EXPECT_TRUE(listener.details_.from_front);
  EXPECT_EQ(1, listener.details_.count);

  // We expect http://www.a.com/0 to be gone.
  EXPECT_EQ(contents->controller()->GetEntryCount(), kMaxEntryCount);
  EXPECT_EQ(contents->controller()->GetEntryAtIndex(0)->url(),
            GURL("test1://www.a.com/1"));

  // More navigations.
  for (int i = 0; i < 3; i++) {
    SNPrintF(buffer, 128, "test1://www.a.com/%d", url_index);
    url = GURL(buffer);
    contents->controller()->LoadURL(url, PageTransition::TYPED);
      contents->CompleteNavigationAsRenderer(url_index, url);
    url_index++;
  }
  EXPECT_EQ(contents->controller()->GetEntryCount(), kMaxEntryCount);
  EXPECT_EQ(contents->controller()->GetEntryAtIndex(0)->url(),
            GURL("test1://www.a.com/4"));

  NavigationController::set_max_entry_count(original_count);
}

// Tests that we can do a restore and navigate to the restored entries and
// everything is updated properly. This can be tricky since there is no
// SiteInstance for the entries created initially.
TEST_F(NavigationControllerTest, RestoreNavigate) {
  site_instance = SiteInstance::CreateSiteInstance(profile);
  site_instance->AddRef();

  // Create a NavigationController with a restored set of tabs.
  GURL url("test1:foo");
  std::vector<TabNavigation> navigations;
  navigations.push_back(TabNavigation(0, url, L"Title", "state",
                                      PageTransition::LINK));
  NavigationController* controller =
      new NavigationController(profile, navigations, 0, NULL);
  controller->GoToIndex(0);

  // We should now have one entry, and it should be "pending".
  EXPECT_EQ(1, controller->GetEntryCount());
  EXPECT_EQ(controller->GetEntryAtIndex(0), controller->GetPendingEntry());
  EXPECT_EQ(0, controller->GetEntryAtIndex(0)->page_id());

  // Say we navigated to that entry.
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url;
  params.transition = PageTransition::LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  NavigationController::LoadCommittedDetails details;
  controller->RendererDidNavigate(params, false, &details);

  // There should be no longer any pending entry and one committed one. This
  // means that we were able to locate the entry, assign its site instance, and
  // commit it properly.
  EXPECT_EQ(1, controller->GetEntryCount());
  EXPECT_EQ(0, controller->GetLastCommittedEntryIndex());
  EXPECT_FALSE(controller->GetPendingEntry());
  EXPECT_EQ(site_instance,
            controller->GetLastCommittedEntry()->site_instance());

  // Clean up the navigation controller.
  ClearContents();
  controller->Destroy();
  site_instance->Release();
}

// Make sure that the page type and stuff is correct after an interstitial.
TEST_F(NavigationControllerTest, Interstitial) {
  // First navigate somewhere normal.
  const GURL url1("test1:foo");
  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  contents->CompleteNavigationAsRenderer(0, url1);

  // Now navigate somewhere with an interstitial.
  const GURL url2("test1:bar");
  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  contents->controller()->GetPendingEntry()->set_page_type(
      NavigationEntry::INTERSTITIAL_PAGE);

  // At this point the interstitial will be displayed and the load will still
  // be pending. If the user continues, the load will commit.
  contents->CompleteNavigationAsRenderer(1, url2);

  // The page should be a normal page again.
  EXPECT_EQ(url2, contents->controller()->GetLastCommittedEntry()->url());
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE,
            contents->controller()->GetLastCommittedEntry()->page_type());
}

// Tests that IsInPageNavigation returns appropriate results.  Prevents
// regression for bug 1126349.
TEST_F(NavigationControllerTest, IsInPageNavigation) {
  // Navigate to URL with no refs.
  const GURL url("http://www.google.com/home.html");
  contents->CompleteNavigationAsRenderer(0, url);

  // Reloading the page is not an in-page navigation.
  EXPECT_FALSE(contents->controller()->IsURLInPageNavigation(url));
  const GURL other_url("http://www.google.com/add.html");
  EXPECT_FALSE(contents->controller()->IsURLInPageNavigation(other_url));
  const GURL url_with_ref("http://www.google.com/home.html#my_ref");
  EXPECT_TRUE(contents->controller()->IsURLInPageNavigation(url_with_ref));

  // Navigate to URL with refs.
  contents->CompleteNavigationAsRenderer(1, url_with_ref);

  // Reloading the page is not an in-page navigation.
  EXPECT_FALSE(contents->controller()->IsURLInPageNavigation(url_with_ref));
  EXPECT_FALSE(contents->controller()->IsURLInPageNavigation(url));
  EXPECT_FALSE(contents->controller()->IsURLInPageNavigation(other_url));
  const GURL other_url_with_ref("http://www.google.com/home.html#my_other_ref");
  EXPECT_TRUE(contents->controller()->IsURLInPageNavigation(
      other_url_with_ref));
}

// A basic test case. Navigates to a single url, and make sure the history
// db matches.
TEST_F(NavigationControllerHistoryTest, Basic) {
  contents->controller()->LoadURL(url0, PageTransition::LINK);
  contents->CompleteNavigationAsRenderer(0, url0);

  GetLastSession();

  helper_.AssertSingleWindowWithSingleTab(windows_, 1);
  helper_.AssertTabEquals(0, 0, 1, *(windows_[0]->tabs[0]));
  TabNavigation nav1(0, url0, std::wstring(), std::string(),
                     PageTransition::LINK);
  helper_.AssertNavigationEquals(nav1, windows_[0]->tabs[0]->navigations[0]);
}

// Navigates to three urls, then goes back and make sure the history database
// is in sync.
TEST_F(NavigationControllerHistoryTest, NavigationThenBack) {
  contents->CompleteNavigationAsRenderer(0, url0);
  contents->CompleteNavigationAsRenderer(1, url1);
  contents->CompleteNavigationAsRenderer(2, url2);

  contents->controller()->GoBack();
  contents->CompleteNavigationAsRenderer(1, url1);

  GetLastSession();

  helper_.AssertSingleWindowWithSingleTab(windows_, 3);
  helper_.AssertTabEquals(0, 1, 3, *(windows_[0]->tabs[0]));

  TabNavigation nav(0, url0, std::wstring(), std::string(),
                    PageTransition::LINK);
  helper_.AssertNavigationEquals(nav, windows_[0]->tabs[0]->navigations[0]);
  nav.url = url1;
  helper_.AssertNavigationEquals(nav, windows_[0]->tabs[0]->navigations[1]);
  nav.url = url2;
  helper_.AssertNavigationEquals(nav, windows_[0]->tabs[0]->navigations[2]);
}

// Navigates to three urls, then goes back twice, then loads a new url.
TEST_F(NavigationControllerHistoryTest, NavigationPruning) {
  contents->CompleteNavigationAsRenderer(0, url0);
  contents->CompleteNavigationAsRenderer(1, url1);
  contents->CompleteNavigationAsRenderer(2, url2);

  contents->controller()->GoBack();
  contents->CompleteNavigationAsRenderer(1, url1);

  contents->controller()->GoBack();
  contents->CompleteNavigationAsRenderer(0, url0);

  contents->CompleteNavigationAsRenderer(3, url2);

  // Now have url0, and url2.

  GetLastSession();

  helper_.AssertSingleWindowWithSingleTab(windows_, 2);
  helper_.AssertTabEquals(0, 1, 2, *(windows_[0]->tabs[0]));

  TabNavigation nav(0, url0, std::wstring(), std::string(),
                    PageTransition::LINK);
  helper_.AssertNavigationEquals(nav, windows_[0]->tabs[0]->navigations[0]);
  nav.url = url2;
  helper_.AssertNavigationEquals(nav, windows_[0]->tabs[0]->navigations[1]);
}
