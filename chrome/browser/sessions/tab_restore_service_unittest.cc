// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/test/test_tab_contents.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabRestoreServiceTest : public testing::Test {
 public:
  TabRestoreServiceTest()
      : tab_contents_factory_(
            TestTabContentsFactory::CreateAndRegisterFactory()),
        profile_(new TestingProfile()),
        service_(new TabRestoreService(profile_.get())) {
    test_contents_ = tab_contents_factory_->CreateInstanceImpl();
    test_contents_->set_commit_on_navigate(true);
    controller_ = new NavigationController(test_contents_, profile_.get());
    url1_ = GURL(tab_contents_factory_->scheme() + "://1");
    url2_ = GURL(tab_contents_factory_->scheme() + "://2");
    url3_ = GURL(tab_contents_factory_->scheme() + "://3");
  }

  ~TabRestoreServiceTest() {
    controller_->Destroy();
  }

 protected:
  void AddThreeNavigations() {
    // Navigate to three URLs.
    controller_->LoadURL(url1_, GURL(), PageTransition::RELOAD);
    controller_->LoadURL(url2_, GURL(), PageTransition::RELOAD);
    controller_->LoadURL(url3_, GURL(), PageTransition::RELOAD);
  }

  void NavigateToIndex(int index) {
    // Navigate back. We have to do this song and dance as NavigationController
    // isn't happy if you navigate immediately while going back.
    test_contents_->set_commit_on_navigate(false);
    controller_->GoToIndex(index);
    test_contents_->CompleteNavigationAsRenderer(
        controller_->GetPendingEntry()->page_id(),
        controller_->GetPendingEntry()->url());
  }

  void RecreateService() {
    // Must set service to null first so that it is destroyed.
    service_ = NULL;
    service_ = new TabRestoreService(profile_.get());
    service_->LoadTabsFromLastSession();
  }

  // Adds a window with one tab and url to the profile's session service.
  void AddWindowWithOneTabToSessionService() {
    SessionService* session_service = profile_->GetSessionService();
    SessionID tab_id;
    SessionID window_id;
    session_service->SetWindowType(window_id, Browser::TYPE_NORMAL);
    session_service->SetTabWindow(window_id, tab_id);
    session_service->SetTabIndexInWindow(window_id, tab_id, 0);
    session_service->SetSelectedTabInWindow(window_id, 0);
    NavigationEntry entry(tab_contents_factory_->type());
    entry.set_url(url1_);
    session_service->UpdateTabNavigation(window_id, tab_id, 0, entry);
  }

  // Creates a SessionService and assigns it to the Profile. The SessionService
  // is configured with a single window with a single tab pointing at url1_ by
  // way of AddWindowWithOneTabToSessionService.
  void CreateSessionServiceWithOneWindow() {
    // The profile takes ownership of this.
    SessionService* session_service = new SessionService(profile_.get());
    profile_->set_session_service(session_service);

    AddWindowWithOneTabToSessionService();

    // Set this, otherwise previous session won't be loaded.
    profile_->set_last_session_exited_cleanly(false);
  }

  GURL url1_;
  GURL url2_;
  GURL url3_;
  scoped_ptr<TestTabContentsFactory> tab_contents_factory_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<TabRestoreService> service_;
  NavigationController* controller_;
  TestTabContents* test_contents_;
};

TEST_F(TabRestoreServiceTest, Basic) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(controller_);

  // Make sure an entry was created.
  ASSERT_EQ(1, service_->entries().size());

  // Make sure the entry matches.
  TabRestoreService::Entry* entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  TabRestoreService::Tab* tab = static_cast<TabRestoreService::Tab*>(entry);
  ASSERT_EQ(3, tab->navigations.size());
  EXPECT_TRUE(url1_ == tab->navigations[0].url());
  EXPECT_TRUE(url2_ == tab->navigations[1].url());
  EXPECT_TRUE(url3_ == tab->navigations[2].url());
  EXPECT_EQ(2, tab->current_navigation_index);

  NavigateToIndex(1);

  // And check again.
  service_->CreateHistoricalTab(controller_);

  // There should be two entries now.
  ASSERT_EQ(2, service_->entries().size());

  // Make sure the entry matches
  entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  tab = static_cast<TabRestoreService::Tab*>(entry);
  ASSERT_EQ(3, tab->navigations.size());
  EXPECT_TRUE(url1_ == tab->navigations[0].url());
  EXPECT_TRUE(url2_ == tab->navigations[1].url());
  EXPECT_TRUE(url3_ == tab->navigations[2].url());
  EXPECT_EQ(1, tab->current_navigation_index);
}

// Make sure TabRestoreService doesn't create an entry for a tab with no
// navigations.
TEST_F(TabRestoreServiceTest, DontCreateEmptyTab) {
  service_->CreateHistoricalTab(controller_);
  EXPECT_TRUE(service_->entries().empty());
}

// Tests restoring a single tab.
TEST_F(TabRestoreServiceTest, Restore) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(controller_);

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1, service_->entries().size());

  // And verify the entry.
  TabRestoreService::Entry* entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  TabRestoreService::Tab* tab = static_cast<TabRestoreService::Tab*>(entry);
  ASSERT_EQ(3, tab->navigations.size());
  EXPECT_TRUE(url1_ == tab->navigations[0].url());
  EXPECT_TRUE(url2_ == tab->navigations[1].url());
  EXPECT_TRUE(url3_ == tab->navigations[2].url());
  EXPECT_EQ(2, tab->current_navigation_index);
}

// Make sure a tab that is restored doesn't come back.
TEST_F(TabRestoreServiceTest, DontLoadRestoredTab) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(controller_);
  ASSERT_EQ(1, service_->entries().size());

  // Restore the tab.
  service_->RestoreEntryById(NULL, service_->entries().front()->id, true);
  ASSERT_TRUE(service_->entries().empty());

  // Recreate the service and have it load the tabs.
  RecreateService();

  // There should be no entries.
  ASSERT_EQ(0, service_->entries().size());
}

// Make sure we don't persist entries to disk that have post data.
TEST_F(TabRestoreServiceTest, DontPersistPostData1) {
  AddThreeNavigations();
  controller_->GetEntryAtIndex(2)->set_has_post_data(true);

  // Have the service record the tab.
  service_->CreateHistoricalTab(controller_);
  ASSERT_EQ(1, service_->entries().size());

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1, service_->entries().size());

  // And verify the entry, the last navigation (url3_) should not have
  // been written to disk as it contained post data.
  TabRestoreService::Entry* entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  TabRestoreService::Tab* tab = static_cast<TabRestoreService::Tab*>(entry);
  ASSERT_EQ(2, tab->navigations.size());
  EXPECT_TRUE(url1_ == tab->navigations[0].url());
  EXPECT_TRUE(url2_ == tab->navigations[1].url());
  EXPECT_EQ(1, tab->current_navigation_index);
}

// Make sure we don't persist entries to disk that have post data. This
// differs from DontPersistPostData1 in that all navigations before the
// current index have post data.
TEST_F(TabRestoreServiceTest, DontPersistPostData2) {
  AddThreeNavigations();
  NavigateToIndex(1);
  controller_->GetEntryAtIndex(0)->set_has_post_data(true);
  controller_->GetEntryAtIndex(1)->set_has_post_data(true);

  // Have the service record the tab.
  service_->CreateHistoricalTab(controller_);
  ASSERT_EQ(1, service_->entries().size());

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_EQ(1, service_->entries().size());

  // And verify the entry, the last navigation (url3_) should not have
  // been written to disk as it contained post data.
  TabRestoreService::Entry* entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  TabRestoreService::Tab* tab = static_cast<TabRestoreService::Tab*>(entry);
  ASSERT_EQ(1, tab->navigations.size());
  EXPECT_TRUE(url3_ == tab->navigations[0].url());
  EXPECT_EQ(0, tab->current_navigation_index);
}

// Make sure we don't persist entries to disk that have post data. This
// differs from DontPersistPostData1 in that all the navigations have post
// data, so that nothing should be persisted.
TEST_F(TabRestoreServiceTest, DontPersistPostData3) {
  AddThreeNavigations();
  controller_->GetEntryAtIndex(0)->set_has_post_data(true);
  controller_->GetEntryAtIndex(1)->set_has_post_data(true);
  controller_->GetEntryAtIndex(2)->set_has_post_data(true);

  // Have the service record the tab.
  service_->CreateHistoricalTab(controller_);
  ASSERT_EQ(1, service_->entries().size());

  // Recreate the service and have it load the tabs.
  RecreateService();

  // One entry should be created.
  ASSERT_TRUE(service_->entries().empty());
}

// Make sure we don't persist entries to disk that have post data. This
// differs from DontPersistPostData1 in that all the navigations have post
// data, so that nothing should be persisted.
TEST_F(TabRestoreServiceTest, DontLoadTwice) {
  AddThreeNavigations();

  // Have the service record the tab.
  service_->CreateHistoricalTab(controller_);
  ASSERT_EQ(1, service_->entries().size());

  // Recreate the service and have it load the tabs.
  RecreateService();

  service_->LoadTabsFromLastSession();

  // There should only be one entry.
  ASSERT_EQ(1, service_->entries().size());
}

// Makes sure we load the previous session as necessary.
TEST_F(TabRestoreServiceTest, LoadPreviousSession) {
  CreateSessionServiceWithOneWindow();

  profile_->GetSessionService()->MoveCurrentSessionToLastSession();

  service_->LoadTabsFromLastSession();

  // Make sure we get back one entry with one tab whose url is url1.
  ASSERT_EQ(1, service_->entries().size());
  TabRestoreService::Entry* entry2 = service_->entries().front();
  ASSERT_EQ(TabRestoreService::WINDOW, entry2->type);
  TabRestoreService::Window* window =
      static_cast<TabRestoreService::Window*>(entry2);
  ASSERT_EQ(1, window->tabs.size());
  EXPECT_EQ(0, window->selected_tab_index);
  ASSERT_EQ(1, window->tabs[0].navigations.size());
  EXPECT_EQ(0, window->tabs[0].current_navigation_index);
  EXPECT_TRUE(url1_ == window->tabs[0].navigations[0].url());
}

// Makes sure we don't attempt to load previous sessions after a restore.
TEST_F(TabRestoreServiceTest, DontLoadAfterRestore) {
  CreateSessionServiceWithOneWindow();

  profile_->GetSessionService()->MoveCurrentSessionToLastSession();

  profile_->set_restored_last_session(true);

  service_->LoadTabsFromLastSession();

  // Because we restored a session TabRestoreService shouldn't load the tabs.
  ASSERT_EQ(0, service_->entries().size());
}

// Makes sure we don't attempt to load previous sessions after a clean exit.
TEST_F(TabRestoreServiceTest, DontLoadAfterCleanExit) {
  CreateSessionServiceWithOneWindow();

  profile_->GetSessionService()->MoveCurrentSessionToLastSession();

  profile_->set_last_session_exited_cleanly(true);

  service_->LoadTabsFromLastSession();

  ASSERT_EQ(0, service_->entries().size());
}

TEST_F(TabRestoreServiceTest, LoadPreviousSessionAndTabs) {
  CreateSessionServiceWithOneWindow();

  profile_->GetSessionService()->MoveCurrentSessionToLastSession();

  AddThreeNavigations();

  service_->CreateHistoricalTab(controller_);

  RecreateService();

  // We should get back two entries, one from the previous session and one from
  // the tab restore service. The previous session entry should be first.
  ASSERT_EQ(2, service_->entries().size());
  // The first entry should come from the session service.
  TabRestoreService::Entry* entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::WINDOW, entry->type);
  TabRestoreService::Window* window =
      static_cast<TabRestoreService::Window*>(entry);
  ASSERT_EQ(1, window->tabs.size());
  EXPECT_EQ(0, window->selected_tab_index);
  ASSERT_EQ(1, window->tabs[0].navigations.size());
  EXPECT_EQ(0, window->tabs[0].current_navigation_index);
  EXPECT_TRUE(url1_ == window->tabs[0].navigations[0].url());

  // Then the closed tab.
  entry = *(++service_->entries().begin());
  ASSERT_EQ(TabRestoreService::TAB, entry->type);
  TabRestoreService::Tab* tab = static_cast<TabRestoreService::Tab*>(entry);
  ASSERT_EQ(3, tab->navigations.size());
  EXPECT_EQ(2, tab->current_navigation_index);
  EXPECT_TRUE(url1_ == tab->navigations[0].url());
  EXPECT_TRUE(url2_ == tab->navigations[1].url());
  EXPECT_TRUE(url3_ == tab->navigations[2].url());
}

// Creates TabRestoreService::kMaxEntries + 1 windows in the session service
// and makes sure we only get back TabRestoreService::kMaxEntries on restore.
TEST_F(TabRestoreServiceTest, ManyWindowsInSessionService) {
  CreateSessionServiceWithOneWindow();

  for (size_t i = 0; i < TabRestoreService::kMaxEntries; ++i)
    AddWindowWithOneTabToSessionService();

  profile_->GetSessionService()->MoveCurrentSessionToLastSession();

  AddThreeNavigations();

  service_->CreateHistoricalTab(controller_);

  RecreateService();

  // We should get back kMaxEntries entries. We added more, but
  // TabRestoreService only allows up to kMaxEntries.
  ASSERT_EQ(TabRestoreService::kMaxEntries, service_->entries().size());

  // The first entry should come from the session service.
  TabRestoreService::Entry* entry = service_->entries().front();
  ASSERT_EQ(TabRestoreService::WINDOW, entry->type);
  TabRestoreService::Window* window =
      static_cast<TabRestoreService::Window*>(entry);
  ASSERT_EQ(1, window->tabs.size());
  EXPECT_EQ(0, window->selected_tab_index);
  ASSERT_EQ(1, window->tabs[0].navigations.size());
  EXPECT_EQ(0, window->tabs[0].current_navigation_index);
  EXPECT_TRUE(url1_ == window->tabs[0].navigations[0].url());
}
