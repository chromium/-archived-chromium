// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/perftimer.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/libxml_utils.h"
#include "chrome/test/automation/autocomplete_edit_proxy.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"

const wchar_t kRunOmniboxTest[] = L"run_omnibox_test";

class OmniboxTest : public UITest {
 public:

  double score_;
  double max_score_;

  int query_count_;
  int query_timeouts_;
  int64 time_squared_;
  int64 time_sum_;
  int64 time_min_;
  int64 time_max_;

  OmniboxTest() : UITest() {
    show_window_ = true;
    query_count_ = 0;
    query_timeouts_ = 0;
    score_ = 0;
    max_score_ = 0;
    time_squared_ = 0;
    time_sum_ = 0;
    time_min_ = 0;
    time_max_ = 0;
  }

  // Many times a user may enter something like "google.com". If
  // http://www.google.com/ is suggested that should be considered a match.
  // This could probably be accomplished with regex as well.  Note that this
  // method is called even when suggestion isn't a URL.
  bool IsMatch(const std::wstring& input_test, const std::wstring& suggestion);
  // Runs a query chain. This sends each proper prefix of the input to the
  // omnibox and scores the autocompelte results returned.
  void RunQueryChain(const std::wstring& input_text);
};

bool OmniboxTest::IsMatch(const std::wstring& input_text,
                          const std::wstring& suggestion) {
  // This prefix list comes from the list used in history_url_provider.cc
  // with the exception of "ftp." and "www.".
  std::wstring prefixes[] = {L"", L"ftp://", L"http://", L"https://",
                             L"ftp.", L"www.", L"ftp://www.", L"ftp://ftp.",
                             L"http://www.", L"https://www."};
  std::wstring postfixes[] = {L"", L"/"};
  for (size_t i = 0; i < arraysize(prefixes); ++i) {
    for (size_t j = 0; j < arraysize(postfixes); ++j) {
      if (prefixes[i] + input_text + postfixes[j] == suggestion)
        return true;
    }
  }
  return false;
}

void OmniboxTest::RunQueryChain(const std::wstring& input_text) {
  // Get a handle on the omnibox and give it focus.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  scoped_refptr<AutocompleteEditProxy> autocomplete_edit(
      browser->GetAutocompleteEdit());
  ASSERT_TRUE(browser->ApplyAccelerator(IDC_FOCUS_LOCATION));

  // Try every proper prefix of input_text.  There's no use trying
  // input_text itself since the autocomplete results will always contain it.
  for (size_t i = 1; i < input_text.size(); ++i) {
    Matches matches;

    // We're only going to count the time elapsed waiting for autocomplete
    // matches to be returned to us.
    ASSERT_TRUE(autocomplete_edit->SetText(input_text.substr(0, i)));
    PerfTimer timer;
    if (autocomplete_edit->WaitForQuery(30000)) {
      ASSERT_TRUE(autocomplete_edit->GetAutocompleteMatches(&matches));
      int64 time_elapsed = timer.Elapsed().InMilliseconds();

      // Adjust statistic trackers.
      if (query_count_ == 0)
        time_min_ = time_max_ = time_elapsed;
      ++query_count_;
      time_squared_ += time_elapsed * time_elapsed;
      time_sum_ += time_elapsed;
      if (time_elapsed < time_min_)
        time_min_ = time_elapsed;
      if (time_elapsed > time_max_)
        time_max_ = time_elapsed;
    } else {
      ++query_timeouts_;
    }
    wprintf(L"query: %d\n", query_count_);

    // Check if any suggestions match the input text.  Stop if a match is
    // found.
    for (Matches::const_iterator j(matches.begin()); j != matches.end(); ++j) {
      if (IsMatch(input_text, j->fill_into_edit)) {
        score_ += i;
        break;
      }
    }
    max_score_ += i;
  }
}

// This test reads in the omnibox_tests.xml file and performs the tests
// within. The current format of xml is fairly simple.  Nothing is currently
// done with the provider information.
// <omnibox_tests>
//   Zero or more test elements.
//   <test query='%query%'>
//     Zero or more provider elements.
//     <provider name='%expected_provider_name%'/>
//   </test>
// </omnibox_tests>

TEST_F(OmniboxTest, Measure) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunOmniboxTest))
    return;

  std::wstring omnibox_tests_path;
  PathService::Get(chrome::DIR_TEST_DATA, &omnibox_tests_path);
  file_util::AppendToPath(&omnibox_tests_path, L"omnibox_tests.xml");

  XmlReader reader;
  ASSERT_TRUE(reader.LoadFile(WideToASCII(omnibox_tests_path)));
  while (reader.SkipToElement()) {
    ASSERT_EQ("omnibox_tests", reader.NodeName());
    reader.Read();
    while (reader.SkipToElement()) {
      ASSERT_EQ("test", reader.NodeName());
      std::string query;
      std::vector<std::string> expected_providers;
      ASSERT_TRUE(reader.NodeAttribute("query", &query));
      reader.Read();
      while (reader.SkipToElement()) {
        ASSERT_EQ("provider", reader.NodeName());
        std::string provider;
        ASSERT_TRUE(reader.NodeAttribute("provider", &provider));
        expected_providers.push_back(provider);
        reader.Read();
      }
      RunQueryChain(ASCIIToWide(query));
      reader.Read();
    }
    reader.Read();
  }

  // Output results.
  ASSERT_GT(query_count_, 0);
  int64 mean = time_sum_ / query_count_;
  wprintf(L"__om_query_count = %d\n", query_count_);
  wprintf(L"__om_query_timeouts = %d\n", query_timeouts_);
  wprintf(L"__om_time_per_query_avg = %d\n", mean);
  // Use the equation stddev = sqrt(Sum(x_i^2)/N - mean^2).
  wprintf(L"__om_time_per_query_stddev = %d\n", static_cast<int64>(
      sqrt(1.0 * time_squared_ / query_count_ - mean * mean)));
  wprintf(L"__om_time_per_query_max = %d\n", time_max_);
  wprintf(L"__om_time_per_query_min = %d\n", time_min_);
  wprintf(L"__om_score = %.4f\n", 100.0 * score_ / max_score_);
}
