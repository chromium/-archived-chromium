// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_parser.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

class TemplateURLParserTest : public testing::Test {
 public:
  TemplateURLParserTest() : parse_result_(true) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &full_path_));
    file_util::AppendToPath(&full_path_, L"osdd");
    if (!file_util::PathExists(full_path_)) {
      LOG(ERROR) <<
          L"This test can't be run without some non-redistributable data";
      full_path_.clear();
    }
  }

  bool IsDisabled() {
    return full_path_.empty();
  }

  // Parses the OpenSearch description document at file_name (relative to
  // the data dir). The TemplateURL is placed in template_url_.
  // The result of Parse is stored in the field parse_result_ (this doesn't
  // use a return value due to internally using ASSERT_).
  void ParseFile(const std::wstring& file_name,
                 TemplateURLParser::ParameterFilter* filter) {
    std::wstring full_path(full_path_);
    file_util::AppendToPath(&full_path, file_name);
    parse_result_ = false;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &full_path));
    file_util::AppendToPath(&full_path, L"osdd");
    file_util::AppendToPath(&full_path, file_name);
    ASSERT_TRUE(file_util::PathExists(full_path));

    std::string contents;
    file_util::ReadFileToString(full_path, &contents);
    parse_result_ = TemplateURLParser::Parse(
        reinterpret_cast<const unsigned char*>(contents.c_str()),
        contents.length(), filter, &template_url_);
  }

  // ParseFile parses the results into this template_url.
  TemplateURL template_url_;

  std::wstring full_path_;

  // Result of the parse.
  bool parse_result_;
};

TEST_F(TemplateURLParserTest, FailOnBogusURL) {
  if (IsDisabled())
    return;
  ParseFile(L"bogus.xml", NULL);
  EXPECT_FALSE(parse_result_);
}

TEST_F(TemplateURLParserTest, PassOnHTTPS) {
  if (IsDisabled())
    return;
  ParseFile(L"https.xml", NULL);
  EXPECT_TRUE(parse_result_);
}

TEST_F(TemplateURLParserTest, FailOnPost) {
  if (IsDisabled())
    return;
  ParseFile(L"post.xml", NULL);
  EXPECT_FALSE(parse_result_);
}

TEST_F(TemplateURLParserTest, TestDictionary) {
  if (IsDisabled())
    return;
  ParseFile(L"dictionary.xml", NULL);
  ASSERT_TRUE(parse_result_);
  EXPECT_EQ(L"Dictionary.com", template_url_.short_name());
  EXPECT_TRUE(template_url_.GetFavIconURL() ==
              GURL("http://cache.lexico.com/g/d/favicon.ico"));
  EXPECT_TRUE(template_url_.url() != NULL);
  EXPECT_TRUE(template_url_.url()->SupportsReplacement());
  EXPECT_EQ(template_url_.url()->url(),
            L"http://dictionary.reference.com/browse/{searchTerms}?r=75");
}

TEST_F(TemplateURLParserTest, TestMSDN) {
  if (IsDisabled())
    return;
  ParseFile(L"msdn.xml", NULL);
  ASSERT_TRUE(parse_result_);
  EXPECT_EQ(L"Search \" MSDN", template_url_.short_name());
  EXPECT_TRUE(template_url_.GetFavIconURL() ==
              GURL("http://search.msdn.microsoft.com/search/favicon.ico"));
  EXPECT_TRUE(template_url_.url() != NULL);
  EXPECT_TRUE(template_url_.url()->SupportsReplacement());
  EXPECT_EQ(template_url_.url()->url(),
            L"http://search.msdn.microsoft.com/search/default.aspx?Query={searchTerms}&brand=msdn&locale=en-US");
}

TEST_F(TemplateURLParserTest, TestWikipedia) {
  if (IsDisabled())
    return;
  ParseFile(L"wikipedia.xml", NULL);
  ASSERT_TRUE(parse_result_);
  EXPECT_EQ(L"Wikipedia (English)", template_url_.short_name());
  EXPECT_TRUE(template_url_.GetFavIconURL() ==
              GURL("http://en.wikipedia.org/favicon.ico"));
  EXPECT_TRUE(template_url_.url() != NULL);
  EXPECT_TRUE(template_url_.url()->SupportsReplacement());
  EXPECT_EQ(template_url_.url()->url(),
      L"http://en.wikipedia.org/w/index.php?title=Special:Search&search={searchTerms}");
  EXPECT_TRUE(template_url_.suggestions_url() != NULL);
  EXPECT_TRUE(template_url_.suggestions_url()->SupportsReplacement());
  EXPECT_EQ(template_url_.suggestions_url()->url(),
      L"http://en.wikipedia.org/w/api.php?action=opensearch&search={searchTerms}");
  ASSERT_EQ(2U, template_url_.input_encodings().size());
  EXPECT_EQ("UTF-8", template_url_.input_encodings()[0]);
  EXPECT_EQ("Shift_JIS", template_url_.input_encodings()[1]);
}

TEST_F(TemplateURLParserTest, NoCrashOnEmptyAttributes) {
  if (IsDisabled())
    return;
  ParseFile(L"url_with_no_attributes.xml", NULL);
}

// Filters any param which as an occurrence of name_str_ in its name or an
// occurrence of value_str_ in its value.
class ParamFilterImpl : public TemplateURLParser::ParameterFilter {
 public:
  ParamFilterImpl(std::string name_str, std::string value_str)
     : name_str_(name_str),
       value_str_(value_str) {
  }

  bool KeepParameter(const std::string& key, const std::string& value) {
    return (name_str_.empty() || key.find(name_str_) == std::string::npos) &&
           (value_str_.empty() || value.find(value_str_) == std::string::npos);
  }

 private:
  std::string name_str_;
  std::string value_str_;

  DISALLOW_EVIL_CONSTRUCTORS(ParamFilterImpl);
};

TEST_F(TemplateURLParserTest, TestFirefoxEbay) {
  if (IsDisabled())
    return;
  // This file uses the Parameter extension
  // (see http://www.opensearch.org/Specifications/OpenSearch/Extensions/Parameter/1.0)
  ParamFilterImpl filter("ebay", "ebay");
  ParseFile(L"firefox_ebay.xml", &filter);
  ASSERT_TRUE(parse_result_);
  EXPECT_EQ(L"eBay", template_url_.short_name());
  EXPECT_TRUE(template_url_.url() != NULL);
  EXPECT_TRUE(template_url_.url()->SupportsReplacement());
  std::wstring exp_url =
      L"http://search.ebay.com/search/search.dll?query={searchTerms}&"
      L"MfcISAPICommand=GetResult&ht=1&srchdesc=n&maxRecordsReturned=300&"
      L"maxRecordsPerPage=50&SortProperty=MetaEndSort";
  EXPECT_EQ(exp_url, template_url_.url()->url());
  ASSERT_EQ(1U, template_url_.input_encodings().size());
  EXPECT_EQ("ISO-8859-1", template_url_.input_encodings()[0]);
  EXPECT_EQ(GURL("http://search.ebay.com/favicon.ico"),
            template_url_.GetFavIconURL());
}

TEST_F(TemplateURLParserTest, TestFirefoxWebster) {
  if (IsDisabled())
    return;
  // This XML file uses a namespace.
  ParamFilterImpl filter("", "Mozilla");
  ParseFile(L"firefox_webster.xml", &filter);
  ASSERT_TRUE(parse_result_);
  EXPECT_EQ(L"Webster", template_url_.short_name());
  EXPECT_TRUE(template_url_.url() != NULL);
  EXPECT_TRUE(template_url_.url()->SupportsReplacement());
  EXPECT_EQ(L"http://www.webster.com/cgi-bin/dictionary?va={searchTerms}",
            template_url_.url()->url());
  ASSERT_EQ(1U, template_url_.input_encodings().size());
  EXPECT_EQ("ISO-8859-1", template_url_.input_encodings()[0]);
  EXPECT_EQ(GURL("http://www.webster.com/favicon.ico"),
            template_url_.GetFavIconURL());
}

TEST_F(TemplateURLParserTest, TestFirefoxYahoo) {
  if (IsDisabled())
    return;
  // This XML file uses a namespace.
  ParamFilterImpl filter("", "Mozilla");
  ParseFile(L"firefox_yahoo.xml", &filter);
  ASSERT_TRUE(parse_result_);
  EXPECT_EQ(L"Yahoo", template_url_.short_name());
  EXPECT_TRUE(template_url_.url() != NULL);
  EXPECT_TRUE(template_url_.url()->SupportsReplacement());
  EXPECT_EQ(L"http://ff.search.yahoo.com/gossip?"
            L"output=fxjson&command={searchTerms}",
            template_url_.suggestions_url()->url());
  EXPECT_EQ(L"http://search.yahoo.com/search?p={searchTerms}&ei=UTF-8",
            template_url_.url()->url());
  ASSERT_EQ(1U, template_url_.input_encodings().size());
  EXPECT_EQ("UTF-8", template_url_.input_encodings()[0]);
  EXPECT_EQ(GURL("http://search.yahoo.com/favicon.ico"),
            template_url_.GetFavIconURL());
}

// Make sure we ignore POST suggestions (this is the same XML file as
// firefox_yahoo.xml, the suggestion method was just changed to POST).
TEST_F(TemplateURLParserTest, TestPostSuggestion) {
  if (IsDisabled())
    return;
  // This XML file uses a namespace.
  ParamFilterImpl filter("", "Mozilla");
  ParseFile(L"post_suggestion.xml", &filter);
  ASSERT_TRUE(parse_result_);
  EXPECT_EQ(L"Yahoo", template_url_.short_name());
  EXPECT_TRUE(template_url_.url() != NULL);
  EXPECT_TRUE(template_url_.url()->SupportsReplacement());
  EXPECT_TRUE(template_url_.suggestions_url() == NULL);
  EXPECT_EQ(L"http://search.yahoo.com/search?p={searchTerms}&ei=UTF-8",
            template_url_.url()->url());
  ASSERT_EQ(1U, template_url_.input_encodings().size());
  EXPECT_EQ("UTF-8", template_url_.input_encodings()[0]);
  EXPECT_EQ(GURL("http://search.yahoo.com/favicon.ico"),
            template_url_.GetFavIconURL());
}
