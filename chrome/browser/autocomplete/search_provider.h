// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the Search autocomplete provider.  This provider is
// responsible for all non-keyword autocomplete entries that start with
// "Search <engine> for ...", including searching for the current input string,
// search history, and search suggestions.  An instance of it gets created and
// managed by the autocomplete controller.
//
// For more information on the autocomplete system in general, including how
// the autocomplete controller and autocomplete providers work, see
// chrome/browser/autocomplete.h.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_SEARCH_PROVIDER_H__
#define CHROME_BROWSER_AUTOCOMPLETE_SEARCH_PROVIDER_H__

#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/url_fetcher.h"

class Profile;
class Value;

// Autocomplete provider for searches and suggestions from a search engine.
//
// After construction, the autocomplete controller repeatedly calls Start()
// with some user input, each time expecting to receive a small set of the best
// matches (either synchronously or asynchronously).
//
// Initially the provider creates a match that searches for the current input
// text.  It also starts a task to query the Suggest servers.  When that data
// comes back, the provider creates and returns matches for the best
// suggestions.
class SearchProvider : public AutocompleteProvider,
                       public URLFetcher::Delegate {
 public:
  SearchProvider(ACProviderListener* listener, Profile* profile)
      : AutocompleteProvider(listener, profile, "Search"),
        last_default_provider_(NULL),
        have_history_results_(false),
        history_request_pending_(false),
        suggest_results_pending_(false),
        fetcher_(NULL),
        have_suggest_results_(false) {
  }

  // AutocompleteProvider
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes);
  virtual void Stop();

  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

 private:
  struct NavigationResult {
    NavigationResult(const GURL& url, const std::wstring& site_name)
        : url(url),
          site_name(site_name) {
    }

    // The URL.
    GURL url;

    // Name for the site.
    std::wstring site_name;
  };

  typedef std::vector<std::wstring> SuggestResults;
  typedef std::vector<NavigationResult> NavigationResults;
  typedef std::vector<history::KeywordSearchTermVisit> HistoryResults;
  typedef std::map<std::wstring, AutocompleteMatch> MatchMap;

  // Called when timer_ expires.
  void Run();

  // Determines whether an asynchronous subcomponent query should run for the
  // current input.  If so, starts it if necessary; otherwise stops it.
  // NOTE: These functions do not update |done_|.  Callers must do so.
  void StartOrStopHistoryQuery(bool minimal_changes);
  void StartOrStopSuggestQuery(bool minimal_changes);

  // Returns true when the current query can be sent to the Suggest service.
  // This will be false e.g. when Suggest is disabled, the query contains
  // potentially private data, etc.
  bool IsQuerySuitableForSuggest() const;

  // Functions to stop the separate asynchronous subcomponents.
  // NOTE: These functions do not update |done_|.  Callers must do so.
  void StopHistory();
  void StopSuggest();

  // Called back by the history system to return searches that begin with the
  // input text.
  void OnGotMostRecentKeywordSearchTerms(
      CancelableRequestProvider::Handle handle,
      HistoryResults* results);

  // Parses the results from the Suggest server and stores up to kMaxMatches of
  // them in server_results_.  Returns whether parsing succeeded.
  bool ParseSuggestResults(Value* root_val);

  // Converts the parsed server results in server_results_ to a set of
  // AutocompleteMatches and adds them to |matches_|.  This also sets |done_|
  // correctly.
  void ConvertResultsToAutocompleteMatches();

  // Determines the relevance for a particular match.  We use different scoring
  // algorithms for the different types of matches.
  int CalculateRelevanceForWhatYouTyped() const;
  // |time| is the time at which this query was last seen.
  int CalculateRelevanceForHistory(const base::Time& time) const;
  // |suggestion_value| is which suggestion this is in the list returned from
  // the server; the best suggestion is suggestion number 0.
  int CalculateRelevanceForSuggestion(size_t suggestion_value) const;
  // |suggestion_value| is same as above.
  int CalculateRelevanceForNavigation(size_t suggestion_value) const;

  // Creates an AutocompleteMatch for "Search <engine> for |query_string|" with
  // the supplied relevance.  Adds this match to |map|; if such a match already
  // exists, whichever one has lower relevance is eliminated.
  void AddMatchToMap(const std::wstring& query_string,
                     int relevance,
                     AutocompleteMatch::Type type,
                     int accepted_suggestion,
                     MatchMap* map);
  // Returns an AutocompleteMatch for a navigational suggestion.
  AutocompleteMatch NavigationToMatch(const NavigationResult& query_string,
                                      int relevance);

  // Trims "http:" and up to two subsequent slashes from |url|.  Returns the
  // number of characters that were trimmed.
  // TODO(kochi): this is duplicate from history_autocomplete
  static size_t TrimHttpPrefix(std::wstring* url);

  // Don't send any queries to the server until some time has elapsed after
  // the last keypress, to avoid flooding the server with requests we are
  // likely to end up throwing away anyway.
  static const int kQueryDelayMs;

  // The user's input.
  AutocompleteInput input_;

  TemplateURL default_provider_;  // Cached across the life of a query so we
                                  // behave consistently even if the user
                                  // changes their default while the query is
                                  // running.
  const TemplateURL* last_default_provider_;
                                  // TODO(pkasting): http://b/1162970  We
                                  // shouldn't need this.

  // An object we can use to cancel history requests.
  CancelableRequestConsumer history_request_consumer_;

  // Searches in the user's history that begin with the input text.
  HistoryResults history_results_;

  // Whether history_results_ is valid (so we can tell invalid apart from
  // empty).
  bool have_history_results_;

  // Whether we are waiting for a history request to finish.
  bool history_request_pending_;

  // True if we're expecting suggest results that haven't yet arrived.  This
  // could be because either |timer_| or |fetcher| is still running (see below).
  bool suggest_results_pending_;

  // A timer to start a query to the suggest server after the user has stopped
  // typing for long enough.
  base::OneShotTimer<SearchProvider> timer_;

  // The fetcher that retrieves suggest results from the server.
  scoped_ptr<URLFetcher> fetcher_;

  // Suggestions returned by the Suggest server for the input text.
  SuggestResults suggest_results_;

  // Navigational suggestions returned by the server.
  NavigationResults navigation_results_;

  // Whether suggest_results_ is valid.
  bool have_suggest_results_;

  DISALLOW_EVIL_CONSTRUCTORS(SearchProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_SEARCH_PROVIDER_H__
