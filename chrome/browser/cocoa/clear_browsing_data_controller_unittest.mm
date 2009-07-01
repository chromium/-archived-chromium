// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/clear_browsing_data_controller.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ClearBrowsingDataControllerTest : public testing::Test {
 public:
  ClearBrowsingDataControllerTest() {
    // Set up some interesting prefs:
    PrefService* prefs = helper_.profile()->GetPrefs();
    prefs->SetBoolean(prefs::kDeleteBrowsingHistory, true);
    prefs->SetBoolean(prefs::kDeleteDownloadHistory, false);
    prefs->SetBoolean(prefs::kDeleteCache, true);
    prefs->SetBoolean(prefs::kDeleteCookies, false);
    prefs->SetBoolean(prefs::kDeletePasswords, true);
    prefs->SetBoolean(prefs::kDeleteFormData, false);
    prefs->SetInteger(prefs::kDeleteTimePeriod,
                      BrowsingDataRemover::FOUR_WEEKS);

    controller_.reset(
      [[ClearBrowsingDataController alloc] initWithProfile:helper_.profile()]);
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  BrowserTestHelper helper_;
  scoped_nsobject<ClearBrowsingDataController> controller_;
};

TEST_F(ClearBrowsingDataControllerTest, InitialState) {
  // Check properties match the prefs set above:
  EXPECT_TRUE([controller_ clearBrowsingHistory]);
  EXPECT_FALSE([controller_ clearDownloadHistory]);
  EXPECT_TRUE([controller_ emptyCache]);
  EXPECT_FALSE([controller_ deleteCookies]);
  EXPECT_TRUE([controller_ clearSavedPasswords]);
  EXPECT_FALSE([controller_ clearFormData]);
  EXPECT_EQ(BrowsingDataRemover::FOUR_WEEKS,
            [controller_ timePeriod]);
}

TEST_F(ClearBrowsingDataControllerTest, InitialRemoveMask) {
  // Check that the remove-mask matches the initial properties:
  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY |
                BrowsingDataRemover::REMOVE_CACHE |
                BrowsingDataRemover::REMOVE_PASSWORDS,
            [controller_ removeMask]);
}

TEST_F(ClearBrowsingDataControllerTest, ModifiedRemoveMask) {
  // Invert all properties and check that the remove-mask is still correct:
  [controller_ setClearBrowsingHistory:false];
  [controller_ setClearDownloadHistory:true];
  [controller_ setEmptyCache:false];
  [controller_ setDeleteCookies:true];
  [controller_ setClearSavedPasswords:false];
  [controller_ setClearFormData:true];

  EXPECT_EQ(BrowsingDataRemover::REMOVE_DOWNLOADS |
                BrowsingDataRemover::REMOVE_COOKIES |
                BrowsingDataRemover::REMOVE_FORM_DATA,
            [controller_ removeMask]);
}

TEST_F(ClearBrowsingDataControllerTest, EmptyRemoveMask) {
  // Clear all properties and check that the remove-mask is zero:
  [controller_ setClearBrowsingHistory:false];
  [controller_ setClearDownloadHistory:false];
  [controller_ setEmptyCache:false];
  [controller_ setDeleteCookies:false];
  [controller_ setClearSavedPasswords:false];
  [controller_ setClearFormData:false];

  EXPECT_EQ(0,
            [controller_ removeMask]);
}

TEST_F(ClearBrowsingDataControllerTest, PersistToPrefs) {
  // Change some settings and store to prefs:
  [controller_ setClearBrowsingHistory:false];
  [controller_ setClearDownloadHistory:true];
  [controller_ persistToPrefs];

  // Test that the modified settings were stored to prefs:
  PrefService* prefs = helper_.profile()->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kDeleteBrowsingHistory));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kDeleteDownloadHistory));

  // Make sure the rest of the prefs didn't change:
  EXPECT_TRUE(prefs->GetBoolean(prefs::kDeleteCache));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kDeleteCookies));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kDeletePasswords));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kDeleteFormData));
  EXPECT_EQ(BrowsingDataRemover::FOUR_WEEKS,
            prefs->GetInteger(prefs::kDeleteTimePeriod));
}

}  // namespace
