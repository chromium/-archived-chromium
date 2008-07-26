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

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/l10n_util.h"
#include "chrome/test/data/resource.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "unicode/locid.h"

namespace {

class L10nUtilTest: public testing::Test {
};

TEST(L10nUtilTest, GetString) {
  std::wstring s = l10n_util::GetString(IDS_SIMPLE);
  EXPECT_EQ(std::wstring(L"Hello World!"), s);

  s = l10n_util::GetStringF(IDS_PLACEHOLDERS, L"chrome", L"10");
  EXPECT_EQ(std::wstring(L"Hello, chrome. Your number is 10."), s);

  s = l10n_util::GetStringF(IDS_PLACEHOLDERS_2, 20);
  EXPECT_EQ(std::wstring(L"You owe me $20."), s);
}

TEST(L10nUtilTest, TruncateString) {
  std::wstring string(L"foooooey    bxxxar baz");

  // Make sure it doesn't modify the string if length > string length.
  EXPECT_EQ(string, l10n_util::TruncateString(string, 100));

  // Test no characters.
  EXPECT_EQ(L"", l10n_util::TruncateString(string, 0));

  // Test 1 character.
  EXPECT_EQ(L"\x2026", l10n_util::TruncateString(string, 1));

  // Test adds ... at right spot when there is enough room to break at a
  // word boundary.
  EXPECT_EQ(L"foooooey\x2026", l10n_util::TruncateString(string, 14));

  // Test adds ... at right spot when there is not enough space in first word.
  EXPECT_EQ(L"f\x2026", l10n_util::TruncateString(string, 2));

  // Test adds ... at right spot when there is not enough room to break at a
  // word boundary.
  EXPECT_EQ(L"foooooey\x2026", l10n_util::TruncateString(string, 11));

  // Test completely truncates string if break is on initial whitespace.
  EXPECT_EQ(L"\x2026", l10n_util::TruncateString(L"   ", 2));
}

void SetICUDefaultLocale(const std::wstring& locale_string) {
  Locale locale(WideToASCII(locale_string).c_str());
  UErrorCode error_code = U_ZERO_ERROR;
  Locale::setDefault(locale, error_code);
  EXPECT_TRUE(U_SUCCESS(error_code));
}

TEST(L10nUtilTest, GetAppLocale) {
  // Use a temporary locale dir so we don't have to actually build the locale
  // dlls for this test.
  std::wstring orig_locale_dir;
  PathService::Get(chrome::DIR_LOCALES, &orig_locale_dir);
  std::wstring new_locale_dir;
  EXPECT_TRUE(file_util::CreateNewTempDirectory(L"l10n_util_test",
                                                &new_locale_dir));
  PathService::Override(chrome::DIR_LOCALES, new_locale_dir);
  // Make fake locale files.
  const wchar_t* filenames[] = {
    L"en-US.dll",
    L"en-GB.dll",
    L"fr.dll",
    L"es-419.dll",
    L"es.dll",
  };
  for (size_t i = 0; i < arraysize(filenames); ++i) {
    std::wstring filename = new_locale_dir;
    file_util::AppendToPath(&filename, filenames[i]);
    file_util::WriteFile(filename, "", 0);
  }

  // Keep a copy of ICU's default locale before we overwrite it.
  Locale locale = Locale::getDefault();

  SetICUDefaultLocale(L"en-US");
  EXPECT_EQ(L"en-US", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale(L"en-GB");
  EXPECT_EQ(L"en-GB", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale(L"fr-CA");
  EXPECT_EQ(L"fr", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale(L"xx");
  EXPECT_EQ(L"en-US", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale(L"en-US");
  EXPECT_EQ(L"fr", l10n_util::GetApplicationLocale(L"fr"));
  EXPECT_EQ(L"fr", l10n_util::GetApplicationLocale(L"fr-CA"));
  // es-419 and es-XX (where XX is not Spain) should be
  // mapped to es-419 (Latin American Spanish).
  EXPECT_EQ(L"es-419", l10n_util::GetApplicationLocale(L"es-419"));
  EXPECT_EQ(L"es", l10n_util::GetApplicationLocale(L"es-ES"));
  EXPECT_EQ(L"es-419", l10n_util::GetApplicationLocale(L"es-AR"));

  SetICUDefaultLocale(L"es-MX");
  EXPECT_EQ(L"es-419", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale(L"es-AR");
  EXPECT_EQ(L"es-419", l10n_util::GetApplicationLocale(L""));
  EXPECT_EQ(L"es", l10n_util::GetApplicationLocale(L"es"));

  SetICUDefaultLocale(L"es-ES");
  EXPECT_EQ(L"es", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale(L"es");
  EXPECT_EQ(L"es", l10n_util::GetApplicationLocale(L""));

  // Clean up.
  PathService::Override(chrome::DIR_LOCALES, orig_locale_dir);
  file_util::Delete(new_locale_dir, true);
  UErrorCode error_code = U_ZERO_ERROR;
  Locale::setDefault(locale, error_code);
}

}
