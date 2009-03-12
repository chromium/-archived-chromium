// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "net/base/escape.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct EscapeCase {
  const wchar_t* input;
  const wchar_t* output;
};

struct UnescapeURLCase {
  const char* input;
  UnescapeRule::Type rules;
  const char* output;
};

struct UnescapeAndDecodeURLCase {
  const char* encoding;
  const char* input;

  // The expected output when run through UnescapeURL.
  const char* url_unescaped;

  // The expected output when run through UnescapeQuery.
  const char* query_unescaped;

  // The expected output when run through UnescapeAndDecodeURLComponent.
  const wchar_t* decoded;
};

struct EscapeForHTMLCase {
  const char* input;
  const char* expected_output;
};

}

TEST(Escape, EscapeTextForFormSubmission) {
  const EscapeCase escape_cases[] = {
    {L"foo", L"foo"},
    {L"foo bar", L"foo+bar"},
    {L"foo++", L"foo%2B%2B"}
  };
  for (size_t i = 0; i < arraysize(escape_cases); ++i) {
    EscapeCase value = escape_cases[i];
    EXPECT_EQ(value.output, EscapeQueryParamValueUTF8(value.input));
  }

  // Test all the values in we're supposed to be escaping.
  const std::string no_escape(
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "!'()*-._~");
  for (int i = 0; i < 256; ++i) {
    std::string in;
    in.push_back(i);
    std::string out = EscapeQueryParamValue(in);
    if (0 == i) {
      EXPECT_EQ(out, std::string("%00"));
    } else if (32 == i) {
      // Spaces are plus escaped like web forms.
      EXPECT_EQ(out, std::string("+"));
    } else if (no_escape.find(in) == std::string::npos) {
      // Check %hex escaping
      std::string expected = StringPrintf("%%%02X", i);
      EXPECT_EQ(expected, out);
    } else {
      // No change for things in the no_escape list.
      EXPECT_EQ(out, in);
    }
  }

  // Check to see if EscapeQueryParamValueUTF8 is the same as
  // EscapeQueryParamValue(..., kCodepageUTF8,)
  std::wstring test_str;
  test_str.reserve(5000);
  for (int i = 1; i < 5000; ++i) {
    test_str.push_back(i);
  }
  std::wstring wide;
  EXPECT_TRUE(EscapeQueryParamValue(test_str, kCodepageUTF8, &wide));
  EXPECT_EQ(wide, EscapeQueryParamValueUTF8(test_str));
}

TEST(Escape, EscapePath) {
  ASSERT_EQ(
    // Most of the character space we care about, un-escaped
    EscapePath(
      "\x02\n\x1d !\"#$%&'()*+,-./0123456789:;"
      "<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "[\\]^_`abcdefghijklmnopqrstuvwxyz"
      "{|}~\x7f\x80\xff"),
    // Escaped
    "%02%0A%1D%20!%22%23$%25&'()*+,-./0123456789%3A;"
    "%3C=%3E%3F@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "%5B%5C%5D%5E_%60abcdefghijklmnopqrstuvwxyz"
    "%7B%7C%7D~%7F%80%FF");
}

TEST(Escape, UnescapeURLComponent) {
  const UnescapeURLCase unescape_cases[] = {
    {"", UnescapeRule::NORMAL, ""},
    {"%2", UnescapeRule::NORMAL, "%2"},
    {"%%%%%%", UnescapeRule::NORMAL, "%%%%%%"},
    {"Don't escape anything", UnescapeRule::NORMAL, "Don't escape anything"},
    {"Invalid %escape %2", UnescapeRule::NORMAL, "Invalid %escape %2"},
    {"Some%20random text %25%3bOK", UnescapeRule::NORMAL,
     "Some%20random text %25;OK"},
    {"Some%20random text %25%3bOK", UnescapeRule::SPACES,
     "Some random text %25;OK"},
    {"Some%20random text %25%3bOK", UnescapeRule::URL_SPECIAL_CHARS,
     "Some%20random text %;OK"},
    {"Some%20random text %25%3bOK",
     UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS,
     "Some random text %;OK"},
    {"%A0%B1%C2%D3%E4%F5", UnescapeRule::NORMAL, "\xA0\xB1\xC2\xD3\xE4\xF5"},
    {"%Aa%Bb%Cc%Dd%Ee%Ff", UnescapeRule::NORMAL, "\xAa\xBb\xCc\xDd\xEe\xFf"},
    // Certain URL-sensitive characters should not be unescaped unless asked.
    {"Hello%20%13%10world %23# %3F? %3D= %26& %25% %2B+", UnescapeRule::SPACES,
     "Hello %13%10world %23# %3F? %3D= %26& %25% %2B+"},
    {"Hello%20%13%10world %23# %3F? %3D= %26& %25% %2B+",
     UnescapeRule::URL_SPECIAL_CHARS,
     "Hello%20%13%10world ## ?? == && %% ++"},
    // Control characters.
    {"%01%02%03%04%05%06%07%08%09 %25", UnescapeRule::URL_SPECIAL_CHARS,
     "%01%02%03%04%05%06%07%08%09 %"},
    {"%01%02%03%04%05%06%07%08%09 %25", UnescapeRule::CONTROL_CHARS,
     "\x01\x02\x03\x04\x05\x06\x07\x08\x09 %25"},
    {"Hello%20%13%10%02", UnescapeRule::SPACES, "Hello %13%10%02"},
    {"Hello%20%13%10%02", UnescapeRule::CONTROL_CHARS, "Hello%20\x13\x10\x02"},
  };

  for (size_t i = 0; i < arraysize(unescape_cases); i++) {
    std::string str(unescape_cases[i].input);
    EXPECT_EQ(std::string(unescape_cases[i].output),
              UnescapeURLComponent(str, unescape_cases[i].rules));
  }

  // Test the NULL character unescaping (which wouldn't work above since those
  // are just char pointers).
  std::string input("Null");
  input.push_back(0);  // Also have a NULL in the input.
  input.append("%00%39Test");

  // When we're unescaping NULLs
  std::string expected("Null");
  expected.push_back(0);
  expected.push_back(0);
  expected.append("9Test");
  EXPECT_EQ(expected, UnescapeURLComponent(input, UnescapeRule::CONTROL_CHARS));

  // When we're not unescaping NULLs.
  expected = "Null";
  expected.push_back(0);
  expected.append("%009Test");
  EXPECT_EQ(expected, UnescapeURLComponent(input, UnescapeRule::NORMAL));
}

TEST(Escape, UnescapeAndDecodeURLComponent) {
  const UnescapeAndDecodeURLCase unescape_cases[] = {
    {"UTF8", "%", "%", "%", L"%"},
    {"UTF8", "+", "+", " ", L"+"},
    {"UTF8", "%2+", "%2+", "%2 ", L"%2+"},
    {"UTF8", "+%%%+%%%", "+%%%+%%%", " %%% %%%", L"+%%%+%%%"},
    {"UTF8", "Don't escape anything",
             "Don't escape anything",
             "Don't escape anything",
             L"Don't escape anything"},
    {"UTF8", "+Invalid %escape %2+",
             "+Invalid %escape %2+",
             " Invalid %escape %2 ",
             L"+Invalid %escape %2+"},
    {"UTF8", "Some random text %25%3bOK",
             "Some random text %25;OK",
             "Some random text %25;OK",
             L"Some random text %25;OK"},
    {"UTF8", "%01%02%03%04%05%06%07%08%09",
             "%01%02%03%04%05%06%07%08%09",
             "%01%02%03%04%05%06%07%08%09",
             L"%01%02%03%04%05%06%07%08%09"},
    {"UTF8", "%E4%BD%A0+%E5%A5%BD",
             "\xE4\xBD\xA0+\xE5\xA5\xBD",
             "\xE4\xBD\xA0 \xE5\xA5\xBD",
             L"\x4f60+\x597d"},
    {"BIG5", "%A7A%A6n",
             "\xA7\x41\xA6n",
             "\xA7\x41\xA6n",
             L"\x4f60\x597d"},
    {"UTF8", "%ED%ED",  // Invalid UTF-8.
             "\xED\xED",
             "\xED\xED",
             L"%ED%ED"},  // Invalid UTF-8 -> kept unescaped.
  };

  for (size_t i = 0; i < arraysize(unescape_cases); i++) {
    std::string unescaped = UnescapeURLComponent(unescape_cases[i].input,
                                                 UnescapeRule::NORMAL);
    EXPECT_EQ(std::string(unescape_cases[i].url_unescaped), unescaped);

    unescaped = UnescapeURLComponent(unescape_cases[i].input,
                                     UnescapeRule::REPLACE_PLUS_WITH_SPACE);
    EXPECT_EQ(std::string(unescape_cases[i].query_unescaped), unescaped);

    // TODO: Need to test unescape_spaces and unescape_percent.
    std::wstring decoded = UnescapeAndDecodeURLComponent(
        unescape_cases[i].input, unescape_cases[i].encoding,
        UnescapeRule::NORMAL);
    EXPECT_EQ(std::wstring(unescape_cases[i].decoded), decoded);
  }
}

TEST(Escape, EscapeForHTML) {
  const EscapeForHTMLCase tests[] = {
    { "hello", "hello" },
    { "<hello>", "&lt;hello&gt;" },
    { "don\'t mess with me", "don&#39;t mess with me" },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string result = EscapeForHTML(std::string(tests[i].input));
    EXPECT_EQ(std::string(tests[i].expected_output), result);
  }
}
