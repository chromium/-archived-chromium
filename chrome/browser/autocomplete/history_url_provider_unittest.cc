// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/history/history.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

struct TestURLInfo {
  std::string url;
  std::wstring title;
  int visit_count;
  int typed_count;
  bool starred;
};

// Contents of the test database.
static TestURLInfo test_db[] = {
  {"http://www.google.com/", L"Google", 3, 3, false},

  // High-quality pages should get a host synthesized as a lower-quality match.
  {"http://slashdot.org/favorite_page.html", L"Favorite page", 200, 100,
   false},

  // Less popular pages should have hosts synthesized as higher-quality
  // matches.
  {"http://kerneltrap.org/not_very_popular.html", L"Less popular", 4, 0,
   false},

  // Unpopular pages should not appear in the results at all.
  {"http://freshmeat.net/unpopular.html", L"Unpopular", 1, 1, false},

  // If a host has a match, we should pick it up during host synthesis.
  {"http://news.google.com/?ned=us&topic=n", L"Google News - U.S.", 2, 2,
   false},
  {"http://news.google.com/", L"Google News", 1, 1, false},

  // Suggested short URLs must be "good enough" and must match user input.
  {"http://foo.com/", L"Dir", 5, 5, false},
  {"http://foo.com/dir/", L"Dir", 2, 2, false},
  {"http://foo.com/dir/another/", L"Dir", 5, 1, false},
  {"http://foo.com/dir/another/again/", L"Dir", 10, 0, false},
  {"http://foo.com/dir/another/again/myfile.html", L"File", 10, 2, false},

  // Starred state is more important than visit count (but less important than
  // typed count) when sorting URLs.  The order in which the URLs were starred
  // shouldn't matter.
  // We throw in a lot of extra URLs here to make sure we're testing the
  // history database's query, not just the autocomplete provider.
  {"http://startest.com/y/a", L"A", 2, 2, true},
  {"http://startest.com/y/b", L"B", 5, 2, false},
  {"http://startest.com/x/c", L"C", 5, 2, true},
  {"http://startest.com/x/d", L"D", 5, 5, false},
  {"http://startest.com/y/e", L"E", 4, 2, false},
  {"http://startest.com/y/f", L"F", 3, 2, false},
  {"http://startest.com/y/g", L"G", 3, 2, false},
  {"http://startest.com/y/h", L"H", 3, 2, false},
  {"http://startest.com/y/i", L"I", 3, 2, false},
  {"http://startest.com/y/j", L"J", 3, 2, false},
  {"http://startest.com/y/k", L"K", 3, 2, false},
  {"http://startest.com/y/l", L"L", 3, 2, false},
  {"http://startest.com/y/m", L"M", 3, 2, false},

  // A file: URL is useful for testing that fixup does the right thing w.r.t.
  // the number of trailing slashes on the user's input.
  {"file:///C:/foo.txt", L"", 2, 2, false},

  // Results with absurdly high typed_counts so that very generic queries like
  // "http" will give consistent results even if more data is added above.
  {"http://bogussite.com/a", L"Bogus A", 10002, 10000, false},
  {"http://bogussite.com/b", L"Bogus B", 10001, 10000, false},
  {"http://bogussite.com/c", L"Bogus C", 10000, 10000, false},
};

class HistoryURLProviderTest : public testing::Test,
                               public ACProviderListener {
  // ACProviderListener
  virtual void OnProviderUpdate(bool updated_matches);

 protected:
  // testing::Test
  virtual void SetUp();
  virtual void TearDown();

  // Fills test data into the history system
  void FillData();

  // Runs an autocomplete query on |text| and checks to see that the returned
  // results' destination URLs match those provided.
  void RunTest(const std::wstring text,
               const std::wstring& desired_tld,
               bool prevent_inline_autocomplete,
               const std::string* expected_urls,
               size_t num_results);

  MessageLoopForUI message_loop_;
  ACMatches matches_;
  scoped_ptr<TestingProfile> profile_;
  HistoryService* history_service_;

 private:
  scoped_refptr<HistoryURLProvider> autocomplete_;
};

void HistoryURLProviderTest::OnProviderUpdate(bool updated_matches) {
  if (autocomplete_->done())
    MessageLoop::current()->Quit();
}

void HistoryURLProviderTest::SetUp() {
  profile_.reset(new TestingProfile());
  profile_->CreateBookmarkModel(true);
  profile_->CreateHistoryService(true);
  history_service_ = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);

  autocomplete_ = new HistoryURLProvider(this, profile_.get());

  FillData();
}

void HistoryURLProviderTest::TearDown() {
  autocomplete_ = NULL;
}

void HistoryURLProviderTest::FillData() {
  // All visits are a long time ago (some tests require this since we do some
  // special logic for things visited very recently). Note that this time must
  // be more recent than the "archived history" threshold for the data to go
  // into the main database.
  //
  // TODO(brettw) It would be nice if we could test this behavior, in which
  // case the time would be specifed in the test_db structure.
  Time visit_time = Time::Now() - TimeDelta::FromDays(80);

  for (size_t i = 0; i < arraysize(test_db); ++i) {
    const TestURLInfo& cur = test_db[i];
    const GURL current_url(cur.url);
    history_service_->AddPageWithDetails(current_url, cur.title,
                                         cur.visit_count, cur.typed_count,
                                         visit_time, false);
    if (cur.starred) {
      profile_->GetBookmarkModel()->SetURLStarred(
          current_url, std::wstring(), true);
    }
  }
}

void HistoryURLProviderTest::RunTest(const std::wstring text,
                                     const std::wstring& desired_tld,
                                     bool prevent_inline_autocomplete,
                                     const std::string* expected_urls,
                                     size_t num_results) {
  AutocompleteInput input(text, desired_tld, prevent_inline_autocomplete,
                          false, false);
  autocomplete_->Start(input, false);
  if (!autocomplete_->done())
    MessageLoop::current()->Run();

  matches_ = autocomplete_->matches();
  ASSERT_EQ(num_results, matches_.size());
  for (size_t i = 0; i < num_results; ++i)
    EXPECT_EQ(expected_urls[i], matches_[i].destination_url.spec());
}

TEST_F(HistoryURLProviderTest, PromoteShorterURLs) {
  // Test that hosts get synthesized below popular pages.
  const std::string expected_nonsynth[] = {
    "http://slash/",
    "http://slashdot.org/favorite_page.html",
    "http://slashdot.org/",
  };
  RunTest(L"slash", std::wstring(), true, expected_nonsynth,
          arraysize(expected_nonsynth));

  // Test that hosts get synthesized above less popular pages.
  const std::string expected_synth[] = {
    "http://kernel/",
    "http://kerneltrap.org/",
    "http://kerneltrap.org/not_very_popular.html",
  };
  RunTest(L"kernel", std::wstring(), true, expected_synth,
          arraysize(expected_synth));

  // Test that unpopular pages are ignored completely.
  const std::string expected_what_you_typed_only[] = {
    "http://fresh/",
  };
  RunTest(L"fresh", std::wstring(), true, expected_what_you_typed_only,
          arraysize(expected_what_you_typed_only));

  // Test that if we have a synthesized host that matches a suggestion, they
  // get combined into one.
  const std::string expected_combine[] = {
    "http://news/",
    "http://news.google.com/",
    "http://news.google.com/?ned=us&topic=n",
  };
  RunTest(L"news", std::wstring(), true, expected_combine,
          arraysize(expected_combine));
  // The title should also have gotten set properly on the host for the
  // synthesized one, since it was also in the results.
  EXPECT_EQ(std::wstring(L"Google News"), matches_[1].description);

  // Test that short URL matching works correctly as the user types more
  // (several tests):
  // The entry for foo.com is the best of all five foo.com* entries.
  const std::string short_1[] = {
    "http://foo/",
    "http://foo.com/",
    "http://foo.com/dir/another/again/myfile.html",
    "http://foo.com/dir/",
  };
  RunTest(L"foo", std::wstring(), true, short_1, arraysize(short_1));

  // When the user types the whole host, make sure we don't get two results for
  // it.
  const std::string short_2[] = {
    "http://foo.com/",
    "http://foo.com/dir/another/again/myfile.html",
    "http://foo.com/dir/",
    "http://foo.com/dir/another/",
  };
  RunTest(L"foo.com", std::wstring(), true, short_2, arraysize(short_2));
  RunTest(L"foo.com/", std::wstring(), true, short_2, arraysize(short_2));

  // The filename is the second best of the foo.com* entries, but there is a
  // shorter URL that's "good enough".  The host doesn't match the user input
  // and so should not appear.
  const std::string short_3[] = {
    "http://foo.com/d",
    "http://foo.com/dir/another/",
    "http://foo.com/dir/another/again/myfile.html",
    "http://foo.com/dir/",
  };
  RunTest(L"foo.com/d", std::wstring(), true, short_3, arraysize(short_3));

  // We shouldn't promote shorter URLs than the best if they're not good
  // enough.
  const std::string short_4[] = {
    "http://foo.com/dir/another/a",
    "http://foo.com/dir/another/again/myfile.html",
    "http://foo.com/dir/another/again/",
  };
  RunTest(L"foo.com/dir/another/a", std::wstring(), true, short_4,
          arraysize(short_4));
}

// Bookmarks have been moved out of the history db, resulting in this no longer
// working. See TODO in URLDatabase::AutocompleteForPrefix.
TEST_F(HistoryURLProviderTest, DISABLED_Starred) {
  // Test that starred pages sort properly.
  const std::string star_1[] = {
    "http://startest/",
    "http://startest.com/x/d",
    "http://startest.com/x/c",
    "http://startest.com/y/a",
  };
  RunTest(L"startest", std::wstring(), true, star_1, arraysize(star_1));
  const std::string star_2[] = {
    "http://startest.com/y",
    "http://startest.com/y/a",
    "http://startest.com/y/b",
    "http://startest.com/y/e",
  };
  RunTest(L"startest.com/y", std::wstring(), true, star_2, arraysize(star_2));
}

TEST_F(HistoryURLProviderTest, CullRedirects) {
  // URLs we will be using, plus the visit counts they will initially get
  // (the redirect set below will also increment the visit counts). We want
  // the results to be in A,B,C order. Note also that our visit counts are
  // all high enough so that domain synthesizing won't get triggered.
  struct RedirectCase {
    const char* url;
    int count;
  };
  static const RedirectCase redirect[] = {
    {"http://redirects/A", 30},
    {"http://redirects/B", 20},
    {"http://redirects/C", 10}
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(redirect); i++) {
    history_service_->AddPageWithDetails(GURL(redirect[i].url), L"Title",
                                         redirect[i].count, redirect[i].count,
                                         Time::Now(), false);
  }

  // Create a B->C->A redirect chain, but set the visit counts such that they
  // will appear in A,B,C order in the results. The autocomplete query will
  // search for the most recent visit when looking for redirects, so this will
  // be found even though the previous visits had no redirects.
  HistoryService::RedirectList redirects_to_a;
  redirects_to_a.push_back(GURL(redirect[1].url));
  redirects_to_a.push_back(GURL(redirect[2].url));
  redirects_to_a.push_back(GURL(redirect[0].url));
  history_service_->AddPage(GURL(redirect[0].url), NULL, 0, GURL(),
                            PageTransition::TYPED, redirects_to_a);

  // Because all the results are part of a redirect chain with other results,
  // all but the first one (A) should be culled. We should get the default
  // "what you typed" result, plus this one.
  const std::wstring typing(L"http://redirects/");
  const std::string expected_results[] = {
    WideToUTF8(typing),
    redirect[0].url};
  RunTest(typing, std::wstring(), true, expected_results,
          arraysize(expected_results));
}

TEST_F(HistoryURLProviderTest, Fixup) {
  // Test for various past crashes we've had.
  RunTest(L"\\", std::wstring(), false, NULL, 0);

  RunTest(L"#", std::wstring(), false, NULL, 0);

  const std::string crash_1[] = {"http://%20/"};
  RunTest(L"%20", std::wstring(), false, crash_1, arraysize(crash_1));

#if defined(OS_WIN)
  // Fixing up "file:" should result in an inline autocomplete offset of just
  // after "file:", not just after "file://".
  const std::wstring input_1(L"file:");
  const std::string fixup_1[] = {"file:///", "file:///C:/foo.txt"};
  RunTest(input_1, std::wstring(), false, fixup_1, arraysize(fixup_1));
  EXPECT_EQ(input_1.length(), matches_[1].inline_autocomplete_offset);

  // Fixing up "http:/" should result in an inline autocomplete offset of just
  // after "http:/", not just after "http:".
  const std::wstring input_2(L"http:/");
  const std::string fixup_2[] = {
    "http://bogussite.com/a",
    "http://bogussite.com/b",
    "http://bogussite.com/c",
  };
  RunTest(input_2, std::wstring(), false, fixup_2, arraysize(fixup_2));
  EXPECT_EQ(input_2.length(), matches_[0].inline_autocomplete_offset);

  // Adding a TLD to a small number like "56" should result in "www.56.com"
  // rather than "0.0.0.56.com".
  std::string fixup_3[] = {"http://www.56.com/"};
  RunTest(L"56", L"com", true, fixup_3, arraysize(fixup_3));
#elif defined(OS_POSIX)
  // TODO(port): Fix this up once the dependencies have their UI bits
  // extracted away.
  NOTIMPLEMENTED();
#endif
}
