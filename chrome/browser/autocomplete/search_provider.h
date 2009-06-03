// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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

#ifndef CHROME_BROWSER_AUTOCOMPLETE_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_SEARCH_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/search_engines/template_url.h"

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
        have_history_results_(false),
        history_request_pending_(false),
        suggest_results_pending_(0),
        have_suggest_results_(false) {
  }

#if defined(UNIT_TEST)
  static void set_query_suggest_immediately(bool value) {
    query_suggest_immediately_ = value;
  }
#endif

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

  // ID used in creating URLFetcher for default provider's suggest results.
  static const int kDefaultProviderURLFetcherID;

  // ID used in creating URLFetcher for keyword provider's suggest results.
  static const int kKeywordProviderURLFetcherID;

 private:
  // Manages the providers (TemplateURLs) used by SearchProvider. Two providers
  // may be used:
  // . The default provider. This corresponds to the user's default search
  //   engine. This is always used, except for the rare case of no default
  //   engine.
  // . The keyword provider. This is used if the user has typed in a keyword.
  class Providers {
   public:
    Providers() : default_provider_(NULL), keyword_provider_(NULL) {}

    // Returns true if the specified providers match the two providers managed
    // by this class.
    bool equals(const TemplateURL* default_provider,
                const TemplateURL* keyword_provider) {
      return (default_provider == default_provider_ &&
              keyword_provider == keyword_provider_);
    }

    // Resets the providers.
    void Set(const TemplateURL* default_provider,
             const TemplateURL* keyword_provider);

    const TemplateURL& default_provider() const {
      DCHECK(valid_default_provider());
      return cached_default_provider_;
    }

    const TemplateURL& keyword_provider() const {
      DCHECK(valid_keyword_provider());
      return cached_keyword_provider_;
    }

    // Returns true of the keyword provider is valid.
    bool valid_keyword_provider() const { return !!keyword_provider_; }

    // Returns true if the keyword provider is valid and has a valid suggest
    // url.
    bool valid_suggest_for_keyword_provider() const {
      return keyword_provider_ && cached_keyword_provider_.suggestions_url();
    }

    // Returns true of the default provider is valid.
    bool valid_default_provider() const { return !!default_provider_; }

    // Returns true if the default provider is valid and has a valid suggest
    // url.
    bool valid_suggest_for_default_provider() const {
      return default_provider_ && cached_default_provider_.suggestions_url();
    }

    // Returns true if |from_keyword_provider| is true, or
    // the keyword provider is not valid.
    bool is_primary_provider(bool from_keyword_provider) const {
      return from_keyword_provider || !valid_keyword_provider();
    }

   private:
    // Cached across the life of a query so we behave consistently even if the
    // user changes their default while the query is running.
    TemplateURL cached_default_provider_;
    TemplateURL cached_keyword_provider_;

    // TODO(pkasting): http://b/1162970  We shouldn't need these.
    const TemplateURL* default_provider_;
    const TemplateURL* keyword_provider_;
  };

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

  // Schedules a history query requesting past searches against the engine
  // whose id is |search_id| and whose text starts with |text|.
  void ScheduleHistoryQuery(TemplateURL::IDType search_id,
                            const std::wstring& text);

  // Called back by the history system to return searches that begin with the
  // input text.
  void OnGotMostRecentKeywordSearchTerms(
      CancelableRequestProvider::Handle handle,
      HistoryResults* results);

  // Creates a URLFetcher requesting suggest results for the specified
  // TemplateURL. Ownership of the returned URLFetchet passes to the caller.
  URLFetcher* CreateSuggestFetcher(int id,
                                   const TemplateURL& provider,
                                   const std::wstring& text);

  // Parses the results from the Suggest server and stores up to kMaxMatches of
  // them in server_results_.  Returns whether parsing succeeded.
  bool ParseSuggestResults(Value* root_val,
                           bool is_keyword,
                           const std::wstring& input_text,
                           SuggestResults* suggest_results);

  // Converts the parsed server results in server_results_ to a set of
  // AutocompleteMatches and adds them to |matches_|.  This also sets |done_|
  // correctly.
  void ConvertResultsToAutocompleteMatches();

  // Converts the first navigation result in |navigation_results| to an
  // AutocompleteMatch and adds it to |matches_|.
  void AddNavigationResultsToMatches(
    const NavigationResults& navigation_results,
    bool is_keyword);

  // Adds a match for each result in |results| to |map|. |is_keyword| indicates
  // whether the results correspond to the keyword provider or default provider.
  void AddHistoryResultsToMap(const HistoryResults& results,
                              bool is_keyword,
                              int did_not_accept_suggestion,
                              MatchMap* map);

  // Adds a match for each result in |suggest_results| to |map|. |is_keyword|
  // indicates whether the results correspond to the keyword provider or default
  // provider.
  void AddSuggestResultsToMap(const SuggestResults& suggest_results,
                              bool is_keyword,
                              int did_not_accept_suggestion,
                              MatchMap* map);

  // Determines the relevance for a particular match.  We use different scoring
  // algorithms for the different types of matches.
  int CalculateRelevanceForWhatYouTyped() const;
  // |time| is the time at which this query was last seen. |is_keyword| is true
  // if the search is from the keyword provider.
  int CalculateRelevanceForHistory(const base::Time& time,
                                   bool is_keyword) const;
  // |suggestion_value| is the index of the suggestion in |suggest_results| that
  // was returned from the server; the best suggestion is suggestion number 0.
  // |is_keyword| is true if the search is from the keyword provider.
  int CalculateRelevanceForSuggestion(const SuggestResults& suggest_results,
                                      size_t suggestion_number,
                                      bool is_keyword) const;
  // |suggestion_value| is same as above. |is_keyword| is true if the navigation
  // result was suggested by the keyword provider.
  int CalculateRelevanceForNavigation(size_t suggestion_value,
                                      bool is_keyword) const;

  // Creates an AutocompleteMatch for "Search <engine> for |query_string|" with
  // the supplied relevance.  Adds this match to |map|; if such a match already
  // exists, whichever one has lower relevance is eliminated.
  void AddMatchToMap(const std::wstring& query_string,
                     int relevance,
                     AutocompleteMatch::Type type,
                     int accepted_suggestion,
                     bool is_keyword,
                     MatchMap* map);
  // Returns an AutocompleteMatch for a navigational suggestion.
  AutocompleteMatch NavigationToMatch(const NavigationResult& query_string,
                                      int relevance,
                                      bool is_keyword);

  // Should we query for suggest results immediately? This is normally false,
  // but may be set to true during testing.
  static bool query_suggest_immediately_;

  // Maintains the TemplateURLs used.
  Providers providers_;

  // The user's input.
  AutocompleteInput input_;

  // Input text when searching against the keyword provider.
  std::wstring keyword_input_text_;

  // An object we can use to cancel history requests. The client data
  // corresponds to the id of the search engine and is used in the callback to
  // determine whether the request corresponds to the keyword of default
  // provider.
  CancelableRequestConsumerTSimple<TemplateURL::IDType>
      history_request_consumer_;

  // Searches in the user's history that begin with the input text.
  HistoryResults keyword_history_results_;
  HistoryResults default_history_results_;

  // Whether history_results_ is valid (so we can tell invalid apart from
  // empty).
  bool have_history_results_;

  // Whether we are waiting for a history request to finish.
  bool history_request_pending_;

  // Number of suggest results that haven't yet arrived. If greater than 0 it
  // indicates either |timer_| or one of the URLFetchers is still running.
  int suggest_results_pending_;

  // A timer to start a query to the suggest server after the user has stopped
  // typing for long enough.
  base::OneShotTimer<SearchProvider> timer_;

  // The fetcher that retrieves suggest results for the keyword from the server.
  scoped_ptr<URLFetcher> keyword_fetcher_;

  // The fetcher that retrieves suggest results for the default engine from the
  // server.
  scoped_ptr<URLFetcher> default_fetcher_;

  // Suggestions returned by the Suggest server for the input text.
  SuggestResults keyword_suggest_results_;
  SuggestResults default_suggest_results_;

  // Navigational suggestions returned by the server.
  NavigationResults keyword_navigation_results_;
  NavigationResults default_navigation_results_;

  // Whether suggest_results_ is valid.
  bool have_suggest_results_;

  DISALLOW_EVIL_CONSTRUCTORS(SearchProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_SEARCH_PROVIDER_H_
