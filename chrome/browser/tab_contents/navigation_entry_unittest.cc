// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

class NavigationEntryTest : public testing::Test {
 public:
  NavigationEntryTest() : instance_(NULL) {
  }

  virtual void SetUp() {
    entry1_.reset(new NavigationEntry);

    instance_ = SiteInstance::CreateSiteInstance(NULL);
    entry2_.reset(new NavigationEntry(instance_, 3,
                                      GURL("test:url"),
                                      GURL("from"),
                                      ASCIIToUTF16("title"),
                                      PageTransition::TYPED));
  }

  virtual void TearDown() {
  }

 protected:
  scoped_ptr<NavigationEntry> entry1_;
  scoped_ptr<NavigationEntry> entry2_;
  // SiteInstances are deleted when their NavigationEntries are gone.
  SiteInstance* instance_;
};

// Test unique ID accessors
TEST_F(NavigationEntryTest, NavigationEntryUniqueIDs) {
  // Two entries should have different IDs by default
  EXPECT_NE(entry1_.get()->unique_id(), entry2_.get()->unique_id());

  // Can set an entry to have the same ID as another
  entry2_.get()->set_unique_id(entry1_.get()->unique_id());
  EXPECT_EQ(entry1_.get()->unique_id(), entry2_.get()->unique_id());
}

// Test URL accessors
TEST_F(NavigationEntryTest, NavigationEntryURLs) {
  // Start with no display_url (even if a url is set)
  EXPECT_FALSE(entry1_.get()->has_display_url());
  EXPECT_FALSE(entry2_.get()->has_display_url());

  EXPECT_EQ(GURL(), entry1_.get()->url());
  EXPECT_EQ(GURL(), entry1_.get()->display_url());
  EXPECT_TRUE(entry1_.get()->GetTitleForDisplay(NULL).empty());

  // Setting URL affects display_url and GetTitleForDisplay
  entry1_.get()->set_url(GURL("http://www.google.com"));
  EXPECT_EQ(GURL("http://www.google.com"), entry1_.get()->url());
  EXPECT_EQ(GURL("http://www.google.com/"), entry1_.get()->display_url());
  EXPECT_EQ(ASCIIToUTF16("http://www.google.com/"),
            entry1_.get()->GetTitleForDisplay(NULL));

  // Title affects GetTitleForDisplay
  entry1_.get()->set_title(ASCIIToUTF16("Google"));
  EXPECT_EQ(ASCIIToUTF16("Google"), entry1_.get()->GetTitleForDisplay(NULL));

  // Setting display_url doesn't affect URL
  entry2_.get()->set_display_url(GURL("display:url"));
  EXPECT_TRUE(entry2_.get()->has_display_url());
  EXPECT_EQ(GURL("test:url"), entry2_.get()->url());
  EXPECT_EQ(GURL("display:url"), entry2_.get()->display_url());

  // Having a title set in constructor overrides display URL
  EXPECT_EQ(ASCIIToUTF16("title"), entry2_.get()->GetTitleForDisplay(NULL));

  // User typed URL is independent of the others
  EXPECT_EQ(GURL(), entry1_.get()->user_typed_url());
  EXPECT_EQ(GURL(), entry2_.get()->user_typed_url());
  entry2_.get()->set_user_typed_url(GURL("typedurl"));
  EXPECT_EQ(GURL("typedurl"), entry2_.get()->user_typed_url());
}

// Test Favicon inner class
TEST_F(NavigationEntryTest, NavigationEntryFavicons) {
  EXPECT_EQ(GURL(), entry1_.get()->favicon().url());
  entry1_.get()->favicon().set_url(GURL("icon"));
  EXPECT_EQ(GURL("icon"), entry1_.get()->favicon().url());

  // Validity not affected by setting URL
  EXPECT_FALSE(entry1_.get()->favicon().is_valid());
  entry1_.get()->favicon().set_is_valid(true);
  EXPECT_TRUE(entry1_.get()->favicon().is_valid());
}

// Test SSLStatus inner class
TEST_F(NavigationEntryTest, NavigationEntrySSLStatus) {
  // Default (not secure)
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN, entry1_.get()->ssl().security_style());
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN, entry2_.get()->ssl().security_style());
  EXPECT_EQ(0, entry1_.get()->ssl().cert_id());
  EXPECT_EQ(0, entry1_.get()->ssl().cert_status());
  EXPECT_EQ(-1, entry1_.get()->ssl().security_bits());
  EXPECT_FALSE(entry1_.get()->ssl().has_mixed_content());
  EXPECT_FALSE(entry1_.get()->ssl().has_unsafe_content());

  // Change from the defaults
  entry2_.get()->ssl().set_security_style(SECURITY_STYLE_AUTHENTICATED);
  entry2_.get()->ssl().set_cert_id(4);
  entry2_.get()->ssl().set_cert_status(1);
  entry2_.get()->ssl().set_security_bits(0);
  entry2_.get()->ssl().set_has_unsafe_content();
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED,
            entry2_.get()->ssl().security_style());
  EXPECT_EQ(4, entry2_.get()->ssl().cert_id());
  EXPECT_EQ(1, entry2_.get()->ssl().cert_status());
  EXPECT_EQ(0, entry2_.get()->ssl().security_bits());
  EXPECT_TRUE(entry2_.get()->ssl().has_unsafe_content());

  // Mixed content unaffected by unsafe content
  EXPECT_FALSE(entry2_.get()->ssl().has_mixed_content());
  entry2_.get()->ssl().set_has_mixed_content();
  EXPECT_TRUE(entry2_.get()->ssl().has_mixed_content());
}

// Test other basic accessors
TEST_F(NavigationEntryTest, NavigationEntryAccessors) {
  // SiteInstance
  EXPECT_TRUE(entry1_.get()->site_instance() == NULL);
  EXPECT_EQ(instance_, entry2_.get()->site_instance());
  entry1_.get()->set_site_instance(instance_);
  EXPECT_EQ(instance_, entry1_.get()->site_instance());

  // Page type
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, entry1_.get()->page_type());
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, entry2_.get()->page_type());
  entry2_.get()->set_page_type(NavigationEntry::INTERSTITIAL_PAGE);
  EXPECT_EQ(NavigationEntry::INTERSTITIAL_PAGE, entry2_.get()->page_type());

  // Referrer
  EXPECT_EQ(GURL(), entry1_.get()->referrer());
  EXPECT_EQ(GURL("from"), entry2_.get()->referrer());
  entry2_.get()->set_referrer(GURL("from2"));
  EXPECT_EQ(GURL("from2"), entry2_.get()->referrer());

  // Title
  EXPECT_EQ(string16(), entry1_.get()->title());
  EXPECT_EQ(ASCIIToUTF16("title"), entry2_.get()->title());
  entry2_.get()->set_title(ASCIIToUTF16("title2"));
  EXPECT_EQ(ASCIIToUTF16("title2"), entry2_.get()->title());

  // State
  EXPECT_EQ(std::string(), entry1_.get()->content_state());
  EXPECT_EQ(std::string(), entry2_.get()->content_state());
  entry2_.get()->set_content_state("state");
  EXPECT_EQ("state", entry2_.get()->content_state());

  // Page ID
  EXPECT_EQ(-1, entry1_.get()->page_id());
  EXPECT_EQ(3, entry2_.get()->page_id());
  entry2_.get()->set_page_id(2);
  EXPECT_EQ(2, entry2_.get()->page_id());

  // Transition type
  EXPECT_EQ(PageTransition::LINK, entry1_.get()->transition_type());
  EXPECT_EQ(PageTransition::TYPED, entry2_.get()->transition_type());
  entry2_.get()->set_transition_type(PageTransition::RELOAD);
  EXPECT_EQ(PageTransition::RELOAD, entry2_.get()->transition_type());

  // Post Data
  EXPECT_FALSE(entry1_.get()->has_post_data());
  EXPECT_FALSE(entry2_.get()->has_post_data());
  entry2_.get()->set_has_post_data(true);
  EXPECT_TRUE(entry2_.get()->has_post_data());

  // Restored
  EXPECT_FALSE(entry1_.get()->restored());
  EXPECT_FALSE(entry2_.get()->restored());
  entry2_.get()->set_restored(true);
  EXPECT_TRUE(entry2_.get()->restored());
}
