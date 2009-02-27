// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/stl_util-inl.h"
#if !defined(OS_MACOSX)
#include "chrome/test/data/resource.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "unicode/locid.h"

namespace {

class StringWrapper {
 public:
  explicit StringWrapper(const std::wstring& string) : string_(string) {}
  const std::wstring& string() const { return string_; }

 private:
  std::wstring string_;

  DISALLOW_COPY_AND_ASSIGN(StringWrapper);
};

}  // namespace

class L10nUtilTest : public PlatformTest {
};

#if defined(OS_WIN)
TEST_F(L10nUtilTest, GetString) {
  std::wstring s = l10n_util::GetString(IDS_SIMPLE);
  EXPECT_EQ(std::wstring(L"Hello World!"), s);

  s = l10n_util::GetStringF(IDS_PLACEHOLDERS, L"chrome", L"10");
  EXPECT_EQ(std::wstring(L"Hello, chrome. Your number is 10."), s);

  s = l10n_util::GetStringF(IDS_PLACEHOLDERS_2, 20);
  EXPECT_EQ(std::wstring(L"You owe me $20."), s);
}
#endif  // defined(OS_WIN)

TEST_F(L10nUtilTest, TruncateString) {
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

TEST_F(L10nUtilTest, GetAppLocale) {
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
    L"en-US",
    L"en-GB",
    L"fr",
    L"es-419",
    L"es",
    L"zh-TW",
    L"zh-CN",
    L"he",
    L"fil",
    L"nb",
  };

#if defined(OS_WIN)
  static const wchar_t kLocaleFileExtension[] = L".dll";
#elif defined(OS_POSIX)
  static const wchar_t kLocaleFileExtension[] = L".pak";
#endif
  for (size_t i = 0; i < arraysize(filenames); ++i) {
    std::wstring filename = new_locale_dir;
    file_util::AppendToPath(&filename, filenames[i]);
    filename += kLocaleFileExtension;
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

  SetICUDefaultLocale(L"en-US");
  // Aliases iw, no, tl to he, nb, fil.
  EXPECT_EQ(L"he", l10n_util::GetApplicationLocale(L"iw"));
  EXPECT_EQ(L"nb", l10n_util::GetApplicationLocale(L"no"));
  EXPECT_EQ(L"fil", l10n_util::GetApplicationLocale(L"tl"));
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

  SetICUDefaultLocale(L"zh-HK");
  EXPECT_EQ(L"zh-TW", l10n_util::GetApplicationLocale(L""));
  EXPECT_EQ(L"zh-CN", l10n_util::GetApplicationLocale(L"zh-CN"));

  SetICUDefaultLocale(L"zh-MK");
  EXPECT_EQ(L"zh-TW", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale(L"zh-SG");
  EXPECT_EQ(L"zh-CN", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale(L"he");
  EXPECT_EQ(L"en-US", l10n_util::GetApplicationLocale(L"en"));

  // Clean up.
  PathService::Override(chrome::DIR_LOCALES, orig_locale_dir);
  file_util::Delete(new_locale_dir, true);
  UErrorCode error_code = U_ZERO_ERROR;
  Locale::setDefault(locale, error_code);
}

TEST_F(L10nUtilTest, SortStringsUsingFunction) {
  std::vector<StringWrapper*> strings;
  strings.push_back(new StringWrapper(L"C"));
  strings.push_back(new StringWrapper(L"d"));
  strings.push_back(new StringWrapper(L"b"));
  strings.push_back(new StringWrapper(L"a"));
  l10n_util::SortStringsUsingMethod(L"en-US", &strings, &StringWrapper::string);
  ASSERT_TRUE(L"a" == strings[0]->string());
  ASSERT_TRUE(L"b" == strings[1]->string());
  ASSERT_TRUE(L"C" == strings[2]->string());
  ASSERT_TRUE(L"d" == strings[3]->string());
  STLDeleteElements(&strings);
}

TEST_F(L10nUtilTest, GetFirstStrongCharacterDirection) {
  // Test pure LTR string.
  std::wstring string(L"foo bar");
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string in which the first character with strong directionality
  // is a character with type L.
  string.assign(L"foo \x05d0 bar");
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string in which the first character with strong directionality
  // is a character with type R.
  string.assign(L"\x05d0 foo bar");
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string which starts with a character with weak directionality
  // and in which the first character with strong directionality is a character
  // with type L.
  string.assign(L"!foo \x05d0 bar");
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string which starts with a character with weak directionality
  // and in which the first character with strong directionality is a character
  // with type R.
  string.assign(L",\x05d0 foo bar");
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string in which the first character with strong directionality
  // is a character with type LRE.
  string.assign(L"\x202a \x05d0 foo  bar");
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string in which the first character with strong directionality
  // is a character with type LRO.
  string.assign(L"\x202d \x05d0 foo  bar");
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string in which the first character with strong directionality
  // is a character with type RLE.
  string.assign(L"\x202b foo \x05d0 bar");
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string in which the first character with strong directionality
  // is a character with type RLO.
  string.assign(L"\x202e foo \x05d0 bar");
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test bidi string in which the first character with strong directionality
  // is a character with type AL.
  string.assign(L"\x0622 foo \x05d0 bar");
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test a string without strong directionality characters.
  string.assign(L",!.{}");
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test empty string.
  string.assign(L"");
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));

  // Test characters in non-BMP (e.g. Phoenician letters. Please refer to
  // http://demo.icu-project.org/icu-bin/ubrowse?scr=151&b=10910 for more
  // information).
#if defined(WCHAR_T_IS_UTF32)
  string.assign(L" ! \x10910" L"abc 123");
#elif defined(WCHAR_T_IS_UTF16)
  string.assign(L" ! \xd802\xdd10" L"abc 123");
#else
#error wchar_t should be either UTF-16 or UTF-32
#endif
  EXPECT_EQ(l10n_util::RIGHT_TO_LEFT,
            l10n_util::GetFirstStrongCharacterDirection(string));

#if defined(WCHAR_T_IS_UTF32)
  string.assign(L" ! \x10401" L"abc 123");
#elif defined(WCHAR_T_IS_UTF16)
  string.assign(L" ! \xd801\xdc01" L"abc 123");
#else
#error wchar_t should be either UTF-16 or UTF-32
#endif
  EXPECT_EQ(l10n_util::LEFT_TO_RIGHT,
            l10n_util::GetFirstStrongCharacterDirection(string));
}

typedef struct {
  std::wstring path;
  std::wstring wrapped_path;
} PathAndWrappedPath;

TEST_F(L10nUtilTest, WrapPathWithLTRFormatting) {
  std::wstring kSeparator;
  kSeparator.push_back(static_cast<wchar_t>(FilePath::kSeparators[0]));
  const PathAndWrappedPath test_data[] = {
    // Test common path, such as "c:\foo\bar".
    { L"c:" + kSeparator + L"foo" + kSeparator + L"bar",
      L"\x202a"L"c:" + kSeparator + L"\x200e"L"foo" + kSeparator +
      L"\x200e"L"bar\x202c"
    },
    // Test path with file name, such as "c:\foo\bar\test.jpg".
    { L"c:" + kSeparator + L"foo" + kSeparator + L"bar" + kSeparator +
      L"test.jpg",
      L"\x202a"L"c:" + kSeparator + L"\x200e"L"foo" + kSeparator +
      L"\x200e"L"bar" + kSeparator + L"\x200e"L"test.jpg\x202c"
    },
    // Test path ending with punctuation, such as "c:\(foo)\bar.".
    { L"c:" + kSeparator + L"(foo)" + kSeparator + L"bar.",
      L"\x202a"L"c:" + kSeparator + L"\x200e"L"(foo)" + kSeparator +
      L"\x200e"L"bar.\x202c"
    },
    // Test path ending with separator, such as "c:\foo\bar\".
    { L"c:" + kSeparator + L"foo" + kSeparator + L"bar" + kSeparator,
      L"\x202a"L"c:" + kSeparator + L"\x200e"L"foo" + kSeparator +
      L"\x200e"L"bar" + kSeparator + L"\x200e\x202c"
    },
    // Test path with RTL character.
    { L"c:" + kSeparator + L"\x05d0",
      L"\x202a"L"c:" + kSeparator + L"\x200e\x05d0\x202c",
    },
    // Test path with 2 level RTL directory names.
    { L"c:" + kSeparator + L"\x05d0" + kSeparator + L"\x0622",
      L"\x202a"L"c:" + kSeparator + L"\x200e\x05d0" + kSeparator +
      L"\x200e\x0622\x202c",
    },
    // Test path with mixed RTL/LTR directory names and ending with punctuation.
    { L"c:" + kSeparator + L"\x05d0" + kSeparator + L"\x0622" + kSeparator +
      L"(foo)" + kSeparator + L"b.a.r.",
      L"\x202a"L"c:" + kSeparator + L"\x200e\x05d0" + kSeparator +
      L"\x200e\x0622" + kSeparator + L"\x200e"L"(foo)" + kSeparator +
      L"\x200e"L"b.a.r.\x202c",
    },
    // Test path without driver name, such as "/foo/bar/test/jpg".
    { kSeparator + L"foo" + kSeparator + L"bar" + kSeparator + L"test.jpg",
      L"\x202a" + kSeparator + L"foo" + kSeparator + L"\x200e" + L"bar" +
      kSeparator + L"\x200e" + L"test.jpg" + L"\x202c"
    },
    // Test path start with current directory, such as "./foo".
    { L"." + kSeparator + L"foo",
      L"\x202a"L"." + kSeparator + L"\x200e" + L"foo" + L"\x202c"
    },
    // Test path start with parent directory, such as "../foo/bar.jpg".
    { L".." + kSeparator + L"foo" + kSeparator + L"bar.jpg",
      L"\x202a"L".." + kSeparator + L"\x200e" + L"foo" + kSeparator +
      L"\x200e" + L"bar.jpg" + L"\x202c"
    },
    // Test absolute path, such as "//foo/bar.jpg".
    { kSeparator + kSeparator + L"foo" + kSeparator + L"bar.jpg",
      L"\x202a" + kSeparator + kSeparator + L"\x200e"L"foo" + kSeparator +
      L"\x200e"L"bar.jpg" + L"\x202c"
    },
    // Test empty path.
    { L"",
      L"\x202a\x202c"
    }
  };
  for (unsigned int i = 0; i < arraysize(test_data); ++i) {
    string16 localized_file_path_string;
    FilePath path = FilePath::FromWStringHack(test_data[i].path);
    l10n_util::WrapPathWithLTRFormatting(path, &localized_file_path_string);
    std::wstring wrapped_path = UTF16ToWide(localized_file_path_string);
    EXPECT_EQ(wrapped_path, test_data[i].wrapped_path);
  }
}
