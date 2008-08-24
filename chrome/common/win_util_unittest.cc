// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/registry.h"
#include "base/string_util.h"
#include "chrome/common/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class WinUtilTest: public testing::Test {
};

// Retrieve the OS primary language
unsigned GetSystemLanguage() {
  RegKey language_key(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Nls\\Language");
  std::wstring language;
  language_key.ReadValue(L"InstallLanguage", &language);
  wchar_t * unused_endptr;
  return PRIMARYLANGID(wcstol(language.c_str(), &unused_endptr, 16));
}

TEST(WinUtilTest, FormatMessage) {
  const int kAccessDeniedErrorCode = 5;
  SetLastError(kAccessDeniedErrorCode);
  ASSERT_EQ(GetLastError(), kAccessDeniedErrorCode);
  std::wstring value;

  unsigned language = GetSystemLanguage();
  ASSERT_TRUE(language);
  if (language == LANG_ENGLISH) {
    // This test would fail on non-English system.
    TrimWhitespace(win_util::FormatLastWin32Error(), TRIM_ALL, &value);
    EXPECT_EQ(value, std::wstring(L"Access is denied."));
  } else if (language == LANG_FRENCH) {
    // This test would fail on non-French system.
    TrimWhitespace(win_util::FormatLastWin32Error(), TRIM_ALL, &value);
    EXPECT_EQ(value, std::wstring(L"Acc\00e8s refus\00e9."));
  } else {
    EXPECT_TRUE(0) << "Please implement the test for your OS language.";
  }

  // Manually call the OS function
  wchar_t * string_buffer = NULL;
  unsigned string_length = ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                    FORMAT_MESSAGE_FROM_SYSTEM |
                                    FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                                    kAccessDeniedErrorCode, 0,
                                    reinterpret_cast<wchar_t *>(&string_buffer),
                                    0, NULL);

  // Verify the call succeeded
  ASSERT_TRUE(string_length);
  ASSERT_TRUE(string_buffer);

  // Verify the string is the same by different calls
  EXPECT_EQ(win_util::FormatLastWin32Error(), std::wstring(string_buffer));
  EXPECT_EQ(win_util::FormatMessage(kAccessDeniedErrorCode),
            std::wstring(string_buffer));

  // Done with the buffer allocated by ::FormatMessage()
  LocalFree(string_buffer);
}

TEST(WinUtilTest, EnsureRectIsVisibleInRect) {
  gfx::Rect parent_rect(0, 0, 500, 400);

  {
    // Child rect x < 0
    gfx::Rect child_rect(-50, 20, 100, 100);
    win_util::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(10, 20, 100, 100), child_rect);
  }

  {
    // Child rect y < 0
    gfx::Rect child_rect(20, -50, 100, 100);
    win_util::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(20, 10, 100, 100), child_rect);
  }

  {
    // Child rect right > parent_rect.right
    gfx::Rect child_rect(450, 20, 100, 100);
    win_util::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(390, 20, 100, 100), child_rect);
  }

  {
    // Child rect bottom > parent_rect.bottom
    gfx::Rect child_rect(20, 350, 100, 100);
    win_util::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(20, 290, 100, 100), child_rect);
  }

  {
    // Child rect width > parent_rect.width
    gfx::Rect child_rect(20, 20, 700, 100);
    win_util::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(20, 20, 480, 100), child_rect);
  }

  {
    // Child rect height > parent_rect.height
    gfx::Rect child_rect(20, 20, 100, 700);
    win_util::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(20, 20, 100, 380), child_rect);
  }
}


