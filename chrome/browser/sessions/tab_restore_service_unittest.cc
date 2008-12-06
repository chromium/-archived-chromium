// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/tab_restore_service.h"
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
