// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/win_util.h"

class BaseWinUtilTest: public testing::Test {
 protected:
  // Retrieve the OS primary language
  static unsigned GetSystemLanguage() {
    std::wstring language;

    typedef BOOL (WINAPI *fnGetThreadPreferredUILanguages)(
        DWORD dwFlags,
        PULONG pulNumLanguages,
        PWSTR pwszLanguagesBuffer,
        PULONG pcchLanguagesBuffer);
    fnGetThreadPreferredUILanguages pGetThreadPreferredUILanguages = NULL;
    pGetThreadPreferredUILanguages =
        reinterpret_cast<fnGetThreadPreferredUILanguages>(
            GetProcAddress(GetModuleHandle(L"kernel32.dll"),
                           "GetThreadPreferredUILanguages"));
    if (pGetThreadPreferredUILanguages) {
      // Vista, MUI-aware.
      ULONG number = 0;
      wchar_t buffer[256] = {0};
      ULONG buffer_size = sizeof(buffer);
      EXPECT_TRUE(pGetThreadPreferredUILanguages(MUI_LANGUAGE_ID, &number,
                                                 buffer, &buffer_size));
      language = buffer;
    } else {
      // XP
      RegKey language_key(HKEY_LOCAL_MACHINE,
                          L"SYSTEM\\CurrentControlSet\\Control\\Nls\\Language");
      language_key.ReadValue(L"InstallLanguage", &language);
    }
    wchar_t * unused_endptr;
    return PRIMARYLANGID(wcstol(language.c_str(), &unused_endptr, 16));
  }
};

// The test is somewhat silly, because the Vista bots some have UAC enabled
// and some have it disabled. At least we check that it does not crash.
TEST_F(BaseWinUtilTest, TestIsUACEnabled) {
  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA) {
    win_util::UserAccountControlIsEnabled();
  } else {
    EXPECT_TRUE(win_util::UserAccountControlIsEnabled());
  }
}

TEST_F(BaseWinUtilTest, TestGetUserSidString) {
  std::wstring user_sid;
  EXPECT_TRUE(win_util::GetUserSidString(&user_sid));
  EXPECT_TRUE(!user_sid.empty());
}

TEST_F(BaseWinUtilTest, TestGetNonClientMetrics) {
  NONCLIENTMETRICS metrics = {0};
  win_util::GetNonClientMetrics(&metrics);
  EXPECT_TRUE(metrics.cbSize > 0);
  EXPECT_TRUE(metrics.iScrollWidth > 0);
  EXPECT_TRUE(metrics.iScrollHeight > 0);
}

TEST_F(BaseWinUtilTest, FormatMessage) {
  unsigned language = GetSystemLanguage();
  ASSERT_TRUE(language);

  const int kAccessDeniedErrorCode = 5;
  SetLastError(kAccessDeniedErrorCode);
  ASSERT_EQ(GetLastError(), kAccessDeniedErrorCode);
  std::wstring value;

  if (language == LANG_ENGLISH) {
    // This test would fail on non-English system.
    TrimWhitespace(win_util::FormatLastWin32Error(), TRIM_ALL, &value);
    EXPECT_EQ(std::wstring(L"Access is denied."), value);
  } else if (language == LANG_FRENCH) {
    // This test would fail on non-French system.
    TrimWhitespace(win_util::FormatLastWin32Error(), TRIM_ALL, &value);
    EXPECT_EQ(std::wstring(L"Acc\u00e8s refus\u00e9."), value);
  } else {
    EXPECT_TRUE(0) << "Please implement the test for your OS language.";
  }

  // Manually call the OS function
  wchar_t * string_buffer = NULL;
  unsigned string_length =
      ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      kAccessDeniedErrorCode, 0,
                      reinterpret_cast<wchar_t *>(&string_buffer), 0, NULL);

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
