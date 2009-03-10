// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/rlz/rlz.h"

#include "base/registry.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Gets rid of registry leftovers from testing. Returns false if there
// is nothing to clean.
bool CleanValue(const wchar_t* key_name, const wchar_t* value) {
  RegKey key;
  if (!key.Open(HKEY_CURRENT_USER, key_name, KEY_READ | KEY_WRITE))
    return false;
  EXPECT_TRUE(key.DeleteValue(value));
  return true;
}

// The chrome events RLZ key lives here.
const wchar_t kKeyName[] = L"Software\\Google\\Common\\Rlz\\Events\\C";

}  // namespace

TEST(RlzLibTest, RecordProductEvent) {
  if (!RLZTracker::InitRlz(base::DIR_EXE))
    return;

  DWORD recorded_value = 0;
  EXPECT_TRUE(RLZTracker::RecordProductEvent(RLZTracker::CHROME,
      RLZTracker::CHROME_OMNIBOX, RLZTracker::FIRST_SEARCH));
  const wchar_t kEvent1[] = L"C1F";
  RegKey key1;
  EXPECT_TRUE(key1.Open(HKEY_CURRENT_USER, kKeyName, KEY_READ));
  EXPECT_TRUE(key1.ReadValueDW(kEvent1, &recorded_value));
  EXPECT_EQ(1, recorded_value);
  EXPECT_TRUE(CleanValue(kKeyName, kEvent1));

  EXPECT_TRUE(RLZTracker::RecordProductEvent(RLZTracker::CHROME,
      RLZTracker::CHROME_HOME_PAGE, RLZTracker::SET_TO_GOOGLE));
  const wchar_t kEvent2[] = L"C2S";
  RegKey key2;
  EXPECT_TRUE(key2.Open(HKEY_CURRENT_USER, kKeyName, KEY_READ));
  DWORD value = 0;
  EXPECT_TRUE(key2.ReadValueDW(kEvent2, &recorded_value));
  EXPECT_EQ(1, recorded_value);
  EXPECT_TRUE(CleanValue(kKeyName, kEvent2));
}

TEST(RlzLibTest, CleanProductEvents) {
  if (!RLZTracker::InitRlz(base::DIR_EXE))
    return;

  DWORD recorded_value = 0;
  EXPECT_TRUE(RLZTracker::RecordProductEvent(RLZTracker::CHROME,
      RLZTracker::CHROME_OMNIBOX, RLZTracker::FIRST_SEARCH));
  const wchar_t kEvent1[] = L"C1F";
  RegKey key1;
  EXPECT_TRUE(key1.Open(HKEY_CURRENT_USER, kKeyName, KEY_READ));
  EXPECT_TRUE(key1.ReadValueDW(kEvent1, &recorded_value));
  EXPECT_EQ(1, recorded_value);

  EXPECT_TRUE(RLZTracker::ClearAllProductEvents(RLZTracker::CHROME));
  EXPECT_FALSE(CleanValue(kKeyName, kEvent1));
}
