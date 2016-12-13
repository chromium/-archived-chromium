// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/net/test_url_fetcher_factory.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/test/testing_profile.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

// The following environment is configured for these tests:
// . The TemplateURL default_t_url_ is set as the default provider.
// . The TemplateURL keyword_t_url_ is added to the TemplateURLModel. This
//   TemplateURL has a valid suggest and search URL.
// . The URL created by using the search term term1_ with default_t_url_ is
//   added to history.
// . The URL created by using the search term keyword_term_ with keyword_t_url_
//   is added to history.
// . test_factory_ is set as the URLFetcher::Factory.
class SearchProviderTest : public testing::Test,
                           public AutocompleteProvider::ACProviderListener {
 public:
  SearchProviderTest()
      : default_t_url_(NULL),
        term1_(L"term1"),
        keyword_t_url_(NULL),
        keyword_term_(L"keyword"),
        quit_when_done_(false) {}

  // See description above class for what this registers.
  virtual void SetUp();

  virtual void TearDown();

 protected:
  // Returns an AutocompleteMatch in provider_'s set of matches that matches
  // |url|. If there is no matching URL, an empty match is returned.
  AutocompleteMatch FindMatchWithDestination(const GURL& url);

  // ACProviderListener method. If we're waiting for the provider to finish,
  // this exits the message loop.
  virtual void OnProviderUpdate(bool updated_matches);

  // Runs a nested message loop until provider_ is done. The message loop is
  // exited by way of OnProviderUPdate.
  void RunTillProviderDone();

  // Invokes Start on provider_, then runs all pending tasks.
  void QueryForInput(const std::wstring& text);

  // See description above class for details of these fields.
  TemplateURL* default_t_url_;
  const std::wstring term1_;
  GURL term1_url_;
  TemplateURL* keyword_t_url_;
  const std::wstring keyword_term_;
  GURL keyword_url_;

  // URLFetcher::Factory implementation registered.
  TestURLFetcherFactory test_factory_;

  MessageLoopForUI message_loop_;

  // Profile we use.
  TestingProfile profile_;

  // The provider.
  scoped_refptr<SearchProvider> provider_;

  // If true, OnProviderUpdate exits out of the current message loop.
  bool quit_when_done_;

  DISALLOW_COPY_AND_ASSIGN(SearchProviderTest);
};

void SearchProviderTest::SetUp() {
  SearchProvider::set_query_suggest_immediately(true);

  // We need both the history service and template url model loaded.
  profile_.CreateHistoryService(true);
  profile_.CreateTemplateURLModel();

  TemplateURLModel* turl_model = profile_.GetTemplateURLModel();

  // Reset the default TemplateURL.
  default_t_url_ = new TemplateURL();
  default_t_url_->SetURL(L"http://defaultturl/{searchTerms}", 0, 0);
  default_t_url_->SetSuggestionsURL(L"http://defaultturl2/{searchTerms}", 0, 0);
  turl_model->Add(default_t_url_);
  turl_model->SetDefaultSearchProvider(default_t_url_);
  TemplateURL::IDType default_provider_id = default_t_url_->id();
  ASSERT_NE(0, default_provider_id);

  // Add url1, with search term term1_.
  HistoryService* history =
      profile_.GetHistoryService(Profile::EXPLICIT_ACCESS);
  term1_url_ = GURL(WideToUTF8(default_t_url_->url()->ReplaceSearchTerms(
      *default_t_url_, term1_, 0, std::wstring())));
  history->AddPageWithDetails(term1_url_, std::wstring(), 1, 1,
                              base::Time::Now(), false);
  history->SetKeywordSearchTermsForURL(term1_url_, default_t_url_->id(),
                                       term1_);

  // Create another TemplateURL.
  keyword_t_url_ = new TemplateURL();
  keyword_t_url_->set_keyword(L"k");
  keyword_t_url_->SetURL(L"http://keyword/{searchTerms}", 0, 0);
  keyword_t_url_->SetSuggestionsURL(L"http://suggest_keyword/{searchTerms}", 0,
                                    0);
  profile_.GetTemplateURLModel()->Add(keyword_t_url_);
  ASSERT_NE(0, keyword_t_url_->id());

  // Add a page and search term for keyword_t_url_.
  keyword_url_ = GURL(WideToUTF8(keyword_t_url_->url()->ReplaceSearchTerms(
      *keyword_t_url_, keyword_term_, 0, std::wstring())));
  history->AddPageWithDetails(keyword_url_, std::wstring(), 1, 1,
                              base::Time::Now(), false);
  history->SetKeywordSearchTermsForURL(keyword_url_, keyword_t_url_->id(),
                                       keyword_term_);

  provider_ = new SearchProvider(this, &profile_);

  URLFetcher::set_factory(&test_factory_);
}

void SearchProviderTest::OnProviderUpdate(bool updated_matches) {
  SearchProvider::set_query_suggest_immediately(false);
  if (quit_when_done_ && provider_->done()) {
    quit_when_done_ = false;
    message_loop_.Quit();
  }
}

void SearchProviderTest::RunTillProviderDone() {
  if (provider_->done())
    return;

  quit_when_done_ = true;
#if defined(OS_WIN)
  message_loop_.Run(NULL);
#elif defined(OS_POSIX)
  message_loop_.Run();
#endif
}

void SearchProviderTest::QueryForInput(const std::wstring& text) {
  // Start a query.
  AutocompleteInput input(text, std::wstring(), false, false, false);
  provider_->Start(input, false);

  // RunAllPending so that the task scheduled by SearchProvider to create the
  // URLFetchers runs.
  message_loop_.RunAllPending();
}

void SearchProviderTest::TearDown() {
  message_loop_.RunAllPending();

  URLFetcher::set_factory(NULL);

  // Shutdown the provider before the profile.
  provider_ = NULL;
}

AutocompleteMatch SearchProviderTest::FindMatchWithDestination(const GURL& url) {
  for (ACMatches::const_iterator i = provider_->matches().begin();
       i != provider_->matches().end(); ++i) {
    if (i->destination_url == url)
      return *i;
  }
  return AutocompleteMatch(NULL, 1, false, AutocompleteMatch::HISTORY_URL);
}

// Tests -----------------------------------------------------------------------

// Make sure we query history for the default provider and a URLFetcher is
// created for the default provider suggest results.
TEST_F(SearchProviderTest, QueryDefaultProvider) {
  std::wstring term = term1_.substr(0, term1_.size() - 1);
  QueryForInput(term);

  // Make sure the default providers suggest service was queried.
  TestURLFetcher* fetcher = test_factory_.GetFetcherByID(
      SearchProvider::kDefaultProviderURLFetcherID);
  ASSERT_TRUE(fetcher);

  // And the URL matches what we expected.
  GURL expected_url = GURL(WideToUTF8(default_t_url_->suggestions_url()->
      ReplaceSearchTerms(*default_t_url_, term, 0, std::wstring())));
  ASSERT_TRUE(fetcher->original_url() == expected_url);

  // Tell the SearchProvider the suggest query is done.
  fetcher->delegate()->OnURLFetchComplete(
      fetcher, GURL(), URLRequestStatus(), 200, ResponseCookies(),
      std::string());
  fetcher = NULL;

  // Run till the history results complete.
  RunTillProviderDone();

  // The SearchProvider is done. Make sure it has a result for the history
  // term term1.
  AutocompleteMatch match = FindMatchWithDestination(term1_url_);
  ASSERT_TRUE(!match.destination_url.is_empty());
}

// Issues a query that matches the registered keyword and makes sure history
// is queried as well as URLFetchers getting created.
TEST_F(SearchProviderTest, QueryKeywordProvider) {
  std::wstring term = keyword_term_.substr(0, keyword_term_.size() - 1);
  QueryForInput(keyword_t_url_->keyword() + L" " + term);

  // Make sure the default providers suggest service was queried.
  TestURLFetcher* default_fetcher = test_factory_.GetFetcherByID(
      SearchProvider::kDefaultProviderURLFetcherID);
  ASSERT_TRUE(default_fetcher);

  // Tell the SearchProvider the default suggest query is done.
  default_fetcher->delegate()->OnURLFetchComplete(
      default_fetcher, GURL(), URLRequestStatus(), 200, ResponseCookies(),
      std::string());
  default_fetcher = NULL;

  // Make sure the keyword providers suggest service was queried.
  TestURLFetcher* keyword_fetcher = test_factory_.GetFetcherByID(
      SearchProvider::kKeywordProviderURLFetcherID);
  ASSERT_TRUE(keyword_fetcher);

  // And the URL matches what we expected.
  GURL expected_url = GURL(WideToUTF8(keyword_t_url_->suggestions_url()->
      ReplaceSearchTerms(*keyword_t_url_, term, 0, std::wstring())));
  ASSERT_TRUE(keyword_fetcher->original_url() == expected_url);

  // Tell the SearchProvider the keyword suggest query is done.
  keyword_fetcher->delegate()->OnURLFetchComplete(
      keyword_fetcher, GURL(), URLRequestStatus(), 200, ResponseCookies(),
      std::string());
  keyword_fetcher = NULL;

  // Run till the history results complete.
  RunTillProviderDone();

  // The SearchProvider is done. Make sure it has a result for the history
  // term keyword.
  AutocompleteMatch match = FindMatchWithDestination(keyword_url_);
  ASSERT_TRUE(!match.destination_url.is_empty());

  // The match should have a TemplateURL.
  EXPECT_TRUE(match.template_url);

  // The fill into edit should contain the keyword.
  EXPECT_EQ(keyword_t_url_->keyword() + L" " + keyword_term_,
            match.fill_into_edit);
}
