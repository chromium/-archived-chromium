// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
