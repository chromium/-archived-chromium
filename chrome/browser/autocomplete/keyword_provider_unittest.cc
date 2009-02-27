// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

class KeywordProviderTest : public testing::Test {
 protected:
  template<class ResultType>
  struct test_data {
    const std::wstring input;
    const size_t num_results;
    const ResultType output[3];
  };

  KeywordProviderTest() : kw_provider_(NULL) { }
  virtual ~KeywordProviderTest() { }

  virtual void SetUp();
  virtual void TearDown();

  template<class ResultType>
  void RunTest(test_data<ResultType>* keyword_cases,
               int num_cases,
               ResultType AutocompleteMatch::* member);

 protected:
  scoped_refptr<KeywordProvider> kw_provider_;
  scoped_ptr<TemplateURLModel> model_;
};

void KeywordProviderTest::SetUp() {
  static const TemplateURLModel::Initializer kTestKeywordData[] = {
    { L"aa", L"aa.com?foo=%s", L"aa" },
    { L"aaaa", L"http://aaaa/?aaaa=1&b=%s&c", L"aaaa" },
    { L"aaaaa", L"%s", L"aaaaa" },
    { L"ab", L"bogus URL %s", L"ab" },
    { L"weasel", L"weasel%sweasel", L"weasel" },
    { L"www", L" +%2B?=%sfoo ", L"www" },
    { L"z", L"%s=z", L"z" },
  };

  model_.reset(new TemplateURLModel(kTestKeywordData, arraysize(kTestKeywordData)));
  kw_provider_ = new KeywordProvider(NULL, model_.get());
}

void KeywordProviderTest::TearDown() {
  model_.reset();
  kw_provider_ = NULL;
}

template<class ResultType>
void KeywordProviderTest::RunTest(
    test_data<ResultType>* keyword_cases,
    int num_cases,
    ResultType AutocompleteMatch::* member) {
  ACMatches matches;
  for (int i = 0; i < num_cases; ++i) {
    AutocompleteInput input(keyword_cases[i].input, std::wstring(), true,
                            false, false);
    kw_provider_->Start(input, false);
    EXPECT_TRUE(kw_provider_->done());
    matches = kw_provider_->matches();
    EXPECT_EQ(keyword_cases[i].num_results, matches.size()) <<
                L"Input was: " + keyword_cases[i].input;
    if (matches.size() == keyword_cases[i].num_results) {
      for (size_t j = 0; j < keyword_cases[i].num_results; ++j) {
        EXPECT_EQ(keyword_cases[i].output[j], matches[j].*member);
      }
    }
  }
}

TEST_F(KeywordProviderTest, Edit) {
  test_data<std::wstring> edit_cases[] = {
    // Searching for a nonexistent prefix should give nothing.
    {L"Not Found",       0, {}},
    {L"aaaaaNot Found",  0, {}},

    // Check that tokenization only collapses whitespace between first tokens,
    // no-query-input cases have a space appended, and action is not escaped.
    {L"z foo",           1, {L"z foo"}},
    {L"z",               1, {L"z "}},
    {L"z    \t",         1, {L"z "}},
    {L"z   a   b   c++", 1, {L"z a   b   c++"}},

    // Matches should be limited to three, and sorted in quality order, not
    // alphabetical.
    {L"aaa",             2, {L"aaaa ", L"aaaaa "}},
    {L"a 1 2 3",         3, {L"aa 1 2 3", L"ab 1 2 3", L"aaaa 1 2 3"}},
    {L"www.a",           3, {L"aa ", L"ab ", L"aaaa "}},
    // Exact matches should prevent returning inexact matches.
    {L"aaaa foo",        1, {L"aaaa foo"}},
    {L"www.aaaa foo",    1, {L"aaaa foo"}},

    // Clean up keyword input properly.
    {L"www",             1, {L"www "}},
    {L"www.",            0, {}},
    {L"www.w w",         2, {L"www w", L"weasel w"}},
    {L"http://www",      1, {L"www "}},
    {L"http://www.",     0, {}},
    {L"ftp: blah",       0, {}},
    {L"mailto:z",        1, {L"z "}},
  };

  RunTest<std::wstring>(edit_cases, arraysize(edit_cases),
                        &AutocompleteMatch::fill_into_edit);
}

TEST_F(KeywordProviderTest, URL) {
  test_data<GURL> url_cases[] = {
    // No query input -> empty destination URL.
    {L"z",               1, {GURL("")}},
    {L"z    \t",         1, {GURL("")}},

    // Check that tokenization only collapses whitespace between first tokens
    // and query input, but not rest of URL, is escaped.
    {L"z   a   b   c++", 1, {GURL("a+++b+++c%2B%2B=z")}},
    {L"www.www www",     1, {GURL(" +%2B?=wwwfoo ")}},

    // Substitution should work with various locations of the "%s".
    {L"aaa 1a2b",        2, {GURL("http://aaaa/?aaaa=1&b=1a2b&c"),
                             GURL("1a2b")}},
    {L"a 1 2 3",         3, {GURL("aa.com?foo=1+2+3"), GURL("bogus URL 1+2+3"),
                             GURL("http://aaaa/?aaaa=1&b=1+2+3&c")}},
    {L"www.w w",         2, {GURL(" +%2B?=wfoo "), GURL("weaselwweasel")}},
  };

  RunTest<GURL>(url_cases, arraysize(url_cases),
                &AutocompleteMatch::destination_url);
}

TEST_F(KeywordProviderTest, Contents) {
  test_data<std::wstring> contents_cases[] = {
    // No query input -> substitute "<enter query>" into contents.
    {L"z",               1, {L"Search z for <enter query>"}},
    {L"z    \t",         1, {L"Search z for <enter query>"}},

    // Check that tokenization only collapses whitespace between first tokens
    // and contents are not escaped or unescaped.
    {L"z   a   b   c++", 1, {L"Search z for a   b   c++"}},
    {L"www.www www",     1, {L"Search www for www"}},

    // Substitution should work with various locations of the "%s".
    {L"aaa",             2, {L"Search aaaa for <enter query>",
                             L"Search aaaaa for <enter query>"}},
    {L"a 1 2 3",         3, {L"Search aa for 1 2 3", L"Search ab for 1 2 3",
                             L"Search aaaa for 1 2 3"}},
    {L"www.w w",         2, {L"Search www for w", L"Search weasel for w"}},
  };

  RunTest<std::wstring>(contents_cases, arraysize(contents_cases),
                        &AutocompleteMatch::contents);
}

TEST_F(KeywordProviderTest, Description) {
  test_data<std::wstring> description_cases[] = {
    // Whole keyword should be returned for both exact and inexact matches.
    {L"z foo",           1, {L"(Keyword: z)"}},
    {L"a foo",           3, {L"(Keyword: aa)", L"(Keyword: ab)",
                             L"(Keyword: aaaa)"}},
    {L"ftp://www.www w", 1, {L"(Keyword: www)"}},

    // Keyword should be returned regardless of query input.
    {L"z",               1, {L"(Keyword: z)"}},
    {L"z    \t",         1, {L"(Keyword: z)"}},
    {L"z   a   b   c++", 1, {L"(Keyword: z)"}},
  };

  RunTest<std::wstring>(description_cases, arraysize(description_cases),
                        &AutocompleteMatch::description);
}

TEST_F(KeywordProviderTest, AddKeyword) {
  TemplateURL* template_url = new TemplateURL();
  std::wstring keyword(L"foo");
  std::wstring url(L"http://www.google.com/foo?q={searchTerms}");
  template_url->SetURL(url, 0, 0);
  template_url->set_keyword(keyword);
  template_url->set_short_name(L"Test");
  model_->Add(template_url);
  ASSERT_TRUE(template_url == model_->GetTemplateURLForKeyword(keyword));
}

TEST_F(KeywordProviderTest, RemoveKeyword) {
  std::wstring url(L"http://aaaa/?aaaa=1&b={searchTerms}&c");
  model_->Remove(model_->GetTemplateURLForKeyword(L"aaaa"));
  ASSERT_TRUE(model_->GetTemplateURLForKeyword(L"aaaa") == NULL);
}
