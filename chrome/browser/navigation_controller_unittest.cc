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
#include "chrome/browser/session_service_test_helper.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/browser/tab_contents_factory.h"
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

class TestContents : public TabContents {
 public:
  BEGIN_MSG_MAP(TestContents)
  END_MSG_MAP()

  TestContents(TabContentsType type) : TabContents(type) {
  }

  // Just record the navigation so it can be checked by the test case
  bool Navigate(const NavigationEntry& entry, bool reload) {
    pending_entry_.reset(new NavigationEntry(entry));
    return true;
  }

  void CompleteNavigation(int page_id) {
    DCHECK(pending_entry_.get());
    pending_entry_->SetPageID(page_id);
    DidNavigateToEntry(pending_entry_.get());
    controller()->NotifyEntryChangedByPageID(type(), NULL, page_id);
    pending_entry_.release();
  }

  NavigationEntry* pending_entry() const { return pending_entry_.get(); }
  void set_pending_entry(NavigationEntry* e) { pending_entry_.reset(e); }

 private:
  scoped_ptr<NavigationEntry> pending_entry_;
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
                              PageTransition::Type,
                              const std::string& override_encoding) {}
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
};

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
  EXPECT_FALSE(contents->controller()->CanStop());
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
  EXPECT_TRUE(contents->pending_entry());
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

  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

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
  EXPECT_TRUE(contents->pending_entry());
  EXPECT_EQ(contents->controller()->GetEntryCount(), 1);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_TRUE(contents->controller()->GetPendingEntry());
  // TODO(darin): maybe this should really be true?
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
  EXPECT_EQ(contents->GetMaxPageID(), 0);

  contents->CompleteNavigation(1);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

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
// new session history entry.
TEST_F(NavigationControllerTest, LoadURL_SamePage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  EXPECT_EQ(0, notifications.size());
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));


  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  EXPECT_EQ(0, notifications.size());
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  // should not have produced a new session history entry
  EXPECT_EQ(contents->controller()->GetEntryCount(), 1);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
}

TEST_F(NavigationControllerTest, LoadURL_Discarded) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");
  const GURL url2("test1:foo2");

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  EXPECT_EQ(0, notifications.size());
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  contents->controller()->LoadURL(url2, PageTransition::TYPED);
  contents->controller()->DiscardPendingEntry();
  EXPECT_EQ(0, notifications.size());

  // should not have produced a new session history entry
  EXPECT_EQ(contents->controller()->GetEntryCount(), 1);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
}

TEST_F(NavigationControllerTest, Reload) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  EXPECT_EQ(0, notifications.size());
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  contents->controller()->Reload();
  EXPECT_EQ(0, notifications.size());

  // the reload is pending...
  EXPECT_EQ(contents->controller()->GetEntryCount(), 1);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), 0);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_TRUE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());

  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  // now the reload is committed...
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
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  contents->controller()->Reload();
  EXPECT_EQ(0, notifications.size());

  contents->pending_entry()->SetURL(url2);
  contents->pending_entry()->SetTransitionType(PageTransition::LINK);
  contents->CompleteNavigation(1);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  // now the reload is committed...
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
  const GURL url2("test1:foo2");

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  contents->controller()->LoadURL(url2, PageTransition::TYPED);
  contents->CompleteNavigation(1);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  contents->controller()->GoBack();
  EXPECT_EQ(0, notifications.size());

  // should now have a pending navigation to go back...
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), 0);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_TRUE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_TRUE(contents->controller()->CanGoForward());

  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  // the back navigation completed successfully
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
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  contents->controller()->LoadURL(url2, PageTransition::TYPED);
  contents->CompleteNavigation(1);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  contents->controller()->GoBack();
  EXPECT_EQ(0, notifications.size());

  // should now have a pending navigation to go back...
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), 0);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_TRUE(contents->controller()->GetPendingEntry());
  EXPECT_FALSE(contents->controller()->CanGoBack());
  EXPECT_TRUE(contents->controller()->CanGoForward());

  contents->pending_entry()->SetURL(url3);
  contents->pending_entry()->SetTransitionType(PageTransition::LINK);
  contents->CompleteNavigation(2);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  // the back navigation resulted in a completely new navigation.
  // TODO(darin): perhaps this behavior will be confusing to users?
  EXPECT_EQ(contents->controller()->GetEntryCount(), 3);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 2);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_TRUE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
}

// Tests what happens when we navigate forward successfully
TEST_F(NavigationControllerTest, Forward) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");
  const GURL url2("test1:foo2");

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  contents->controller()->LoadURL(url2, PageTransition::TYPED);
  contents->CompleteNavigation(1);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  contents->controller()->GoBack();
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  contents->controller()->GoForward();

  // should now have a pending navigation to go forward...
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), 1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_TRUE(contents->controller()->GetPendingEntry());
  EXPECT_TRUE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());

  contents->CompleteNavigation(1);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  // the forward navigation completed successfully
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

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  contents->controller()->LoadURL(url2, PageTransition::TYPED);
  contents->CompleteNavigation(1);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  contents->controller()->GoBack();
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  contents->controller()->GoForward();
  EXPECT_EQ(0, notifications.size());

  // should now have a pending navigation to go forward...
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), 1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_TRUE(contents->controller()->GetPendingEntry());
  EXPECT_TRUE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());

  contents->pending_entry()->SetURL(url3);
  contents->pending_entry()->SetTransitionType(PageTransition::LINK);
  contents->CompleteNavigation(2);
  EXPECT_TRUE(notifications.Check3AndReset(NOTIFY_NAV_LIST_PRUNED,
                                           NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_TRUE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
}

TEST_F(NavigationControllerTest, LinkClick) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo1");
  const GURL url2("test1:foo2");

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  contents->set_pending_entry(new NavigationEntry(kTestContentsType1, NULL, 0,
                                                  url2,
                                                  std::wstring(),
                                                  PageTransition::LINK));
  contents->CompleteNavigation(1);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  // should not have produced a new session history entry
  EXPECT_EQ(contents->controller()->GetEntryCount(), 2);
  EXPECT_EQ(contents->controller()->GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(contents->controller()->GetPendingEntryIndex(), -1);
  EXPECT_TRUE(contents->controller()->GetLastCommittedEntry());
  EXPECT_FALSE(contents->controller()->GetPendingEntry());
  EXPECT_TRUE(contents->controller()->CanGoBack());
  EXPECT_FALSE(contents->controller()->CanGoForward());
}

TEST_F(NavigationControllerTest, SwitchTypes) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, contents->controller());

  const GURL url1("test1:foo");
  const GURL url2("test2:foo");

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

  TestContents* initial_contents = contents;

  contents->controller()->LoadURL(url2, PageTransition::TYPED);

  // The tab contents should have been replaced
  ASSERT_TRUE(initial_contents != contents);

  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

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
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

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

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  contents->CompleteNavigation(0);
  EXPECT_TRUE(notifications.Check2AndReset(NOTIFY_NAV_ENTRY_COMMITTED,
                                           NOTIFY_NAV_ENTRY_CHANGED));

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

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  contents->CompleteNavigation(0);

  contents->controller()->LoadURL(url2, PageTransition::TYPED);
  contents->CompleteNavigation(0);

  contents->controller()->LoadURL(url3, PageTransition::TYPED);
  contents->CompleteNavigation(1);

  // Navigate back to the start
  contents->controller()->GoToIndex(0);
  contents->CompleteNavigation(0);

  // Now jump to the end
  contents->controller()->GoToIndex(2);
  contents->CompleteNavigation(1);

  // There may be TabContentsCollector tasks pending, so flush them from queue.
  MessageLoop::current()->RunAllPending();

  // Now that the tasks have been flushed, the first tab type should be gone.
  ASSERT_TRUE(
      contents->controller()->GetTabContents(kTestContentsType1) == NULL);
  ASSERT_EQ(contents, 
      contents->controller()->GetTabContents(kTestContentsType2));
}

// Tests that we limit the number of navigation entries created correctly.
TEST_F(NavigationControllerTest, EnforceMaxNavigationCount) {
  const size_t kMaxEntryCount = 5;

  contents->controller()->max_entry_count_ = kMaxEntryCount;

  int url_index;
  char buffer[128];
  // Load up to the max count, all entries should be there.
  for (url_index = 0; url_index < kMaxEntryCount; url_index++) {
    SNPrintF(buffer, 128, "test1://www.a.com/%d", url_index);
    contents->controller()->LoadURL(GURL(buffer), PageTransition::TYPED);
    contents->CompleteNavigation(url_index);
  }

  EXPECT_EQ(contents->controller()->GetEntryCount(), kMaxEntryCount);

  // Navigate some more.
  SNPrintF(buffer, 128, "test1://www.a.com/%d", url_index);
  contents->controller()->LoadURL(GURL(buffer), PageTransition::TYPED);
  contents->CompleteNavigation(url_index);
  url_index++;

  // We expect http://www.a.com/0 to be gone.
  EXPECT_EQ(contents->controller()->GetEntryCount(), kMaxEntryCount);
  EXPECT_EQ(contents->controller()->GetEntryAtIndex(0)->GetURL(),
            GURL("test1://www.a.com/1"));

  // More navigations.
  for (int i = 0; i < 3; i++) {
    SNPrintF(buffer, 128, "test1://www.a.com/%d", url_index);
    contents->controller()->LoadURL(GURL(buffer), PageTransition::TYPED);
    contents->CompleteNavigation(url_index);
    url_index++;
  }
  EXPECT_EQ(contents->controller()->GetEntryCount(), kMaxEntryCount);
  EXPECT_EQ(contents->controller()->GetEntryAtIndex(0)->GetURL(),
            GURL("test1://www.a.com/4"));
}

// A basic test case. Navigates to a single url, and make sure the history
// db matches.
TEST_F(NavigationControllerHistoryTest, Basic) {
  contents->controller()->LoadURL(url0, PageTransition::TYPED);
  contents->CompleteNavigation(0);

  GetLastSession();

  helper_.AssertSingleWindowWithSingleTab(windows_, 1);
  helper_.AssertTabEquals(0, 0, 1, *(windows_[0]->tabs[0]));
  TabNavigation nav1(0, url0, std::wstring(), std::string(),
                     PageTransition::TYPED);
  helper_.AssertNavigationEquals(nav1, windows_[0]->tabs[0]->navigations[0]);
}

// Navigates to three urls, then goes back and make sure the history database
// is in sync.
TEST_F(NavigationControllerHistoryTest, NavigationThenBack) {
  contents->controller()->LoadURL(url0, PageTransition::TYPED);
  contents->CompleteNavigation(0);

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  contents->CompleteNavigation(1);

  contents->controller()->LoadURL(url2, PageTransition::TYPED);
  contents->CompleteNavigation(2);

  contents->controller()->GoBack();
  contents->CompleteNavigation(1);

  GetLastSession();

  helper_.AssertSingleWindowWithSingleTab(windows_, 3);
  helper_.AssertTabEquals(0, 1, 3, *(windows_[0]->tabs[0]));

  TabNavigation nav(0, url0, std::wstring(), std::string(),
                    PageTransition::TYPED);
  helper_.AssertNavigationEquals(nav, windows_[0]->tabs[0]->navigations[0]);
  nav.index = 1;
  nav.url = url1;
  helper_.AssertNavigationEquals(nav, windows_[0]->tabs[0]->navigations[1]);
  nav.index = 2;
  nav.url = url2;
  helper_.AssertNavigationEquals(nav, windows_[0]->tabs[0]->navigations[2]);
}

// Navigates to three urls, then goes back twice, then loads a new url.
TEST_F(NavigationControllerHistoryTest, NavigationPruning) {
  contents->controller()->LoadURL(url0, PageTransition::TYPED);
  contents->CompleteNavigation(0);

  contents->controller()->LoadURL(url1, PageTransition::TYPED);
  contents->CompleteNavigation(1);

  contents->controller()->LoadURL(url2, PageTransition::TYPED);
  contents->CompleteNavigation(2);

  contents->controller()->GoBack();
  contents->CompleteNavigation(1);

  contents->controller()->GoBack();
  contents->CompleteNavigation(0);

  contents->controller()->LoadURL(url2, PageTransition::TYPED);
  contents->CompleteNavigation(3);

  // Now have url0, and url2.

  GetLastSession();

  helper_.AssertSingleWindowWithSingleTab(windows_, 2);
  helper_.AssertTabEquals(0, 1, 2, *(windows_[0]->tabs[0]));

  TabNavigation nav(0, url0, std::wstring(), std::string(),
                    PageTransition::TYPED);
  helper_.AssertNavigationEquals(nav, windows_[0]->tabs[0]->navigations[0]);
  nav.index = 1;
  nav.url = url2;
  helper_.AssertNavigationEquals(nav, windows_[0]->tabs[0]->navigations[1]);
}

