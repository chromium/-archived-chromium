// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/template_url.h"
#include "testing/gtest/include/gtest/gtest.h"

class TemplateURLTest : public testing::Test {
 public:
  virtual void TearDown() {
    delete TemplateURLRef::google_base_url_;
    TemplateURLRef::google_base_url_ = NULL;
  }

  void CheckSuggestBaseURL(const wchar_t* base_url,
                           const wchar_t* base_suggest_url) const {
    delete TemplateURLRef::google_base_url_;
    TemplateURLRef::google_base_url_ = new std::wstring(base_url);
    EXPECT_STREQ(base_suggest_url,
                 TemplateURLRef::GoogleBaseSuggestURLValue().c_str());
  }
};

TEST_F(TemplateURLTest, Defaults) {
  TemplateURL url;
  ASSERT_FALSE(url.show_in_default_list());
  ASSERT_FALSE(url.safe_for_autoreplace());
  ASSERT_EQ(0, url.prepopulate_id());
}

TEST_F(TemplateURLTest, TestValidWithComplete) {
  TemplateURLRef ref(L"{searchTerms}", 0, 0);
  ASSERT_TRUE(ref.IsValid());
}

TEST_F(TemplateURLTest, URLRefTestSearchTerms) {
  TemplateURL t_url;
  TemplateURLRef ref(L"http://foo{searchTerms}", 0, 0);
  ASSERT_TRUE(ref.IsValid());

  ASSERT_TRUE(ref.SupportsReplacement());
  ASSERT_EQ(L"http://foosearch", ref.ReplaceSearchTerms(t_url, L"search",
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()));
}

TEST_F(TemplateURLTest, URLRefTestCount) {
  TemplateURL t_url;
  TemplateURLRef ref(L"http://foo{searchTerms}{count?}", 0, 0);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  ASSERT_EQ(L"http://fooX", ref.ReplaceSearchTerms(t_url, L"X",
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()));
}

TEST_F(TemplateURLTest, URLRefTestCount2) {
  TemplateURL t_url;
  TemplateURLRef ref(L"http://foo{searchTerms}{count}", 0, 0);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  ASSERT_EQ(L"http://fooX10", ref.ReplaceSearchTerms(t_url, L"X",
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()));
}

TEST_F(TemplateURLTest, URLRefTestIndices) {
  TemplateURL t_url;
  TemplateURLRef ref(L"http://foo{searchTerms}x{startIndex?}y{startPage?}",
                     1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  ASSERT_EQ(L"http://fooXxy", ref.ReplaceSearchTerms(t_url, L"X",
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()));
}

TEST_F(TemplateURLTest, URLRefTestIndices2) {
  TemplateURL t_url;
  TemplateURLRef ref(L"http://foo{searchTerms}x{startIndex}y{startPage}", 1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  ASSERT_EQ(L"http://fooXx1y2", ref.ReplaceSearchTerms(t_url, L"X",
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()));
}

TEST_F(TemplateURLTest, URLRefTestEncoding) {
  TemplateURL t_url;
  TemplateURLRef ref(
      L"http://foo{searchTerms}x{inputEncoding?}y{outputEncoding?}a", 1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  ASSERT_EQ(L"http://fooXxUTF-8ya", ref.ReplaceSearchTerms(t_url, L"X",
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()));
}

TEST_F(TemplateURLTest, InputEncodingBeforeSearchTerm) {
  TemplateURL t_url;
  TemplateURLRef ref(
      L"http://foox{inputEncoding?}a{searchTerms}y{outputEncoding?}b", 1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  ASSERT_EQ(L"http://fooxUTF-8aXyb", ref.ReplaceSearchTerms(t_url, L"X",
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()));
}

TEST_F(TemplateURLTest, URLRefTestEncoding2) {
  TemplateURL t_url;
  TemplateURLRef ref(
      L"http://foo{searchTerms}x{inputEncoding}y{outputEncoding}a", 1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  ASSERT_EQ(L"http://fooXxUTF-8yUTF-8a", ref.ReplaceSearchTerms(t_url, L"X",
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()));
}

TEST_F(TemplateURLTest, URLRefTermToWide) {
  struct ToWideCase {
    const char* encoded_search_term;
    const wchar_t* expected_decoded_term;
  } to_wide_cases[] = {
    {"hello+world", L"hello world"},
    // Test some big-5 input.
    {"%a7A%A6%6e+to+you", L"\x4f60\x597d to you"},
    // Test some UTF-8 input. We should fall back to this when the encoding
    // doesn't look like big-5. We have a '5' in the middle, which is an invalid
    // Big-5 trailing byte.
    {"%e4%bd%a05%e5%a5%bd+to+you", L"\x4f60\x35\x597d to you"},
    // Undecodable input should stay escaped.
    {"%91%01+abcd", L"%91%01 abcd"},
  };

  TemplateURL t_url;

  // Set one input encoding: big-5. This is so we can test fallback to UTF-8.
  std::vector<std::string> encodings;
  encodings.push_back("big-5");
  t_url.set_input_encodings(encodings);

  TemplateURLRef ref(L"http://foo?q={searchTerms}", 1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());

  for (int i = 0; i < arraysize(to_wide_cases); i++) {
    std::wstring result = ref.SearchTermToWide(t_url,
        to_wide_cases[i].encoded_search_term);

    EXPECT_EQ(std::wstring(to_wide_cases[i].expected_decoded_term), result);
  }
}

TEST_F(TemplateURLTest, SetFavIcon) {
  TemplateURL url;
  GURL favicon_url("http://favicon.url");
  url.SetFavIconURL(favicon_url);
  ASSERT_EQ(1, url.image_refs().size());
  ASSERT_TRUE(favicon_url == url.GetFavIconURL());

  GURL favicon_url2("http://favicon2.url");
  url.SetFavIconURL(favicon_url2);
  ASSERT_EQ(1, url.image_refs().size());
  ASSERT_TRUE(favicon_url2 == url.GetFavIconURL());
}

TEST_F(TemplateURLTest, DisplayURLToURLRef) {
  struct TestData {
    const std::wstring url;
    const std::wstring expected_result;
  } data[] = {
    { L"http://foo{searchTerms}x{inputEncoding}y{outputEncoding}a",
      L"http://foo%sx{inputEncoding}y{outputEncoding}a" },
    { L"http://X",
      L"http://X" },
    { L"http://foo{searchTerms",
      L"http://foo{searchTerms" },
    { L"http://foo{searchTerms}{language}",
      L"http://foo%s{language}" },
  };
  for (int i = 0; i < arraysize(data); ++i) {
    TemplateURLRef ref(data[i].url, 1, 2);
    EXPECT_EQ(data[i].expected_result, ref.DisplayURL());
    EXPECT_EQ(data[i].url,
              TemplateURLRef::DisplayURLToURLRef(ref.DisplayURL()));
  }
}

TEST_F(TemplateURLTest, ReplaceSearchTerms) {
  struct TestData {
    const std::wstring url;
    const std::wstring expected_result;
  } data[] = {
    { L"{language}{searchTerms}{inputEncoding}",
      L"{language}XUTF-8" },
    { L"{language}{inputEncoding}{searchTerms}",
      L"{language}UTF-8X" },
    { L"{searchTerms}{language}{inputEncoding}",
      L"X{language}UTF-8" },
    { L"{searchTerms}{inputEncoding}{language}",
      L"XUTF-8{language}" },
    { L"{inputEncoding}{searchTerms}{language}",
      L"UTF-8X{language}" },
    { L"{inputEncoding}{language}{searchTerms}",
      L"UTF-8{language}X" },
    { L"{language}a{searchTerms}a{inputEncoding}a",
      L"{language}aXaUTF-8a" },
    { L"{language}a{inputEncoding}a{searchTerms}a",
      L"{language}aUTF-8aXa" },
    { L"{searchTerms}a{language}a{inputEncoding}a",
      L"Xa{language}aUTF-8a" },
    { L"{searchTerms}a{inputEncoding}a{language}a",
      L"XaUTF-8a{language}a" },
    { L"{inputEncoding}a{searchTerms}a{language}a",
      L"UTF-8aXa{language}a" },
    { L"{inputEncoding}a{language}a{searchTerms}a",
      L"UTF-8a{language}aXa" },
  };
  TemplateURL turl;
  turl.add_input_encoding("UTF-8");
  for (int i = 0; i < arraysize(data); ++i) {
    TemplateURLRef ref(data[i].url, 1, 2);
    std::wstring expected_result = data[i].expected_result;
    ReplaceSubstringsAfterOffset(&expected_result, 0, L"{language}",
                                 g_browser_process->GetApplicationLocale());
    EXPECT_EQ(expected_result, ref.ReplaceSearchTerms(turl, L"X",
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()));
  }
}


// Tests replacing search terms in various encodings and making sure the
// generated URL matches the expected value.
TEST_F(TemplateURLTest, ReplaceArbitrarySearchTerms) {
  struct TestData {
    const std::string encoding;
    const std::wstring search_term;
    const std::wstring url;
    const std::wstring expected_result;
  } data[] = {
    { "BIG5",  L"\x60BD", L"{searchTerms}{inputEncoding}", L"%B1~BIG5" },
    { "UTF-8", L"blah",   L"{searchTerms}{inputEncoding}", L"blahUTF-8" },
  };
  for (int i = 0; i < arraysize(data); ++i) {
    TemplateURL turl;
    turl.add_input_encoding(data[i].encoding);
    TemplateURLRef ref(data[i].url, 1, 2);
    std::wstring expected_result = data[i].expected_result;
    EXPECT_EQ(data[i].expected_result, ref.ReplaceSearchTerms(
        turl, data[i].search_term, TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
        std::wstring()));
  }
}

TEST_F(TemplateURLTest, Suggestions) {
  struct TestData {
    const int accepted_suggestion;
    const std::wstring original_query_for_suggestion;
    const std::wstring expected_result;
  } data[] = {
    { TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring(),
      L"foo?q=foobar" },
    { TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, L"foo", L"foo?q=foobar" },
    { TemplateURLRef::NO_SUGGESTION_CHOSEN, std::wstring(),
      L"foo?aq=f&q=foobar" },
    { TemplateURLRef::NO_SUGGESTION_CHOSEN, L"foo", L"foo?aq=f&q=foobar" },
    { 0, std::wstring(), L"foo?aq=0&oq=&q=foobar" },
    { 1, L"foo", L"foo?aq=1&oq=foo&q=foobar" },
  };
  TemplateURL turl;
  turl.add_input_encoding("UTF-8");
  TemplateURLRef ref(L"foo?{google:acceptedSuggestion}"
      L"{google:originalQueryForSuggestion}q={searchTerms}", 1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  for (int i = 0; i < arraysize(data); ++i) {
    EXPECT_EQ(data[i].expected_result, ref.ReplaceSearchTerms(turl, L"foobar",
        data[i].accepted_suggestion, data[i].original_query_for_suggestion));
  }
}

TEST_F(TemplateURLTest, RLZ) {
  std::wstring rlz_string;
  RLZTracker::GetAccessPointRlz(RLZTracker::CHROME_OMNIBOX, &rlz_string);

  TemplateURL t_url;
  TemplateURLRef ref(L"{google:RLZ}{searchTerms}", 1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  ASSERT_EQ(rlz_string + L"x", ref.ReplaceSearchTerms(t_url, L"x",
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()));
}

TEST_F(TemplateURLTest, HostAndSearchTermKey) {
  struct TestData {
    const std::wstring url;
    const std::string host;
    const std::string path;
    const std::string search_term_key;
  } data[] = {
    { L"http://blah/?foo=bar&q={searchTerms}&b=x", "blah", "/", "q"},

    // No query key should result in empty values.
    { L"http://blah/{searchTerms}", "", "", ""},

    // No term should result in empty values.
    { L"http://blah/", "", "", ""},

    // Multiple terms should result in empty values.
    { L"http://blah/?q={searchTerms}&x={searchTerms}", "", "", ""},

    // Term in the host shouldn't match.
    { L"http://{searchTerms}", "", "", ""},

    { L"http://blah/?q={searchTerms}", "blah", "/", "q"},

    // Single term with extra chars in value should match.
    { L"http://blah/?q=stock:{searchTerms}", "blah", "/", "q"},

  };

  TemplateURL t_url;
  for (int i = 0; i < arraysize(data); ++i) {
    t_url.SetURL(data[i].url, 0, 0);
    EXPECT_EQ(data[i].host, t_url.url()->GetHost());
    EXPECT_EQ(data[i].path, t_url.url()->GetPath());
    EXPECT_EQ(data[i].search_term_key, t_url.url()->GetSearchTermKey());
  }
}

TEST_F(TemplateURLTest, GoogleBaseSuggestURL) {
  static const struct {
    const wchar_t* const base_url;
    const wchar_t* const base_suggest_url;
  } data[] = {
    { L"http://google.com/", L"http://clients1.google.com/complete/", },
    { L"http://www.google.com/", L"http://clients1.google.com/complete/", },
    { L"http://www.google.co.uk/", L"http://clients1.google.co.uk/complete/", },
    { L"http://www.google.com.by/",
      L"http://clients1.google.com.by/complete/", },
    { L"http://google.com/intl/xx/", L"http://clients1.google.com/complete/", },
  };

  for (int i = 0; i < arraysize(data); ++i)
    CheckSuggestBaseURL(data[i].base_url, data[i].base_suggest_url);
}

TEST_F(TemplateURLTest, Keyword) {
  TemplateURL t_url;
  t_url.SetURL(L"http://www.google.com/search", 0, 0);
  EXPECT_FALSE(t_url.autogenerate_keyword());
  t_url.set_keyword(L"foo");
  EXPECT_EQ(L"foo", t_url.keyword());
  t_url.set_autogenerate_keyword(true);
  EXPECT_TRUE(t_url.autogenerate_keyword());
  EXPECT_EQ(L"google.com", t_url.keyword());
  t_url.set_keyword(L"foo");
  EXPECT_FALSE(t_url.autogenerate_keyword());
  EXPECT_EQ(L"foo", t_url.keyword());
}

