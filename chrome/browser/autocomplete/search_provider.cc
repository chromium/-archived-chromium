// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/search_provider.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google_util.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/url_util.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_status.h"

using base::Time;
using base::TimeDelta;

// static
const int SearchProvider::kDefaultProviderURLFetcherID = 1;
// static
const int SearchProvider::kKeywordProviderURLFetcherID = 2;

// static
bool SearchProvider::query_suggest_immediately_ = false;

void SearchProvider::Providers::Set(const TemplateURL* default_provider,
                                    const TemplateURL* keyword_provider) {
  // TODO(pkasting): http://b/1162970  We shouldn't need to structure-copy
  // this. Nor should we need |default_provider_| and |keyword_provider_|
  // just to know whether the provider changed.
  default_provider_ = default_provider;
  if (default_provider)
    cached_default_provider_ = *default_provider;
  keyword_provider_ = keyword_provider;
  if (keyword_provider)
    cached_keyword_provider_ = *keyword_provider;
}

void SearchProvider::Start(const AutocompleteInput& input,
                           bool minimal_changes) {
  matches_.clear();

  // Can't return search/suggest results for bogus input or without a profile.
  if (!profile_ || (input.type() == AutocompleteInput::INVALID)) {
    Stop();
    return;
  }

  keyword_input_text_.clear();
  const TemplateURL* keyword_provider =
      KeywordProvider::GetSubstitutingTemplateURLForInput(profile_, input,
                                                          &keyword_input_text_);
  if (!TemplateURL::SupportsReplacement(keyword_provider) ||
      keyword_input_text_.empty()) {
    keyword_provider = NULL;
  }

  const TemplateURL* default_provider =
      profile_->GetTemplateURLModel()->GetDefaultSearchProvider();
  if (!TemplateURL::SupportsReplacement(default_provider))
    default_provider = NULL;

  if (keyword_provider == default_provider)
    keyword_provider = NULL;  // No use in querying the same provider twice.

  if (!default_provider && !keyword_provider) {
    // No valid providers.
    Stop();
    return;
  }

  // If we're still running an old query but have since changed the query text
  // or the providers, abort the query.
  if (!done_ && (!minimal_changes ||
                 !providers_.equals(default_provider, keyword_provider))) {
    Stop();
  }

  providers_.Set(default_provider, keyword_provider);

  if (input.text().empty()) {
    // User typed "?" alone.  Give them a placeholder result indicating what
    // this syntax does.
    if (default_provider) {
      AutocompleteMatch match(this, 0, false,
                              AutocompleteMatch::SEARCH_WHAT_YOU_TYPED);
      static const std::wstring kNoQueryInput(
          l10n_util::GetString(IDS_AUTOCOMPLETE_NO_QUERY));
      match.contents.assign(l10n_util::GetStringF(
          IDS_AUTOCOMPLETE_SEARCH_CONTENTS,
          default_provider->AdjustedShortNameForLocaleDirection(),
          kNoQueryInput));
      match.contents_class.push_back(
          ACMatchClassification(0, ACMatchClassification::DIM));
      matches_.push_back(match);
    }
    Stop();
    return;
  }

  input_ = input;

  StartOrStopHistoryQuery(minimal_changes);
  StartOrStopSuggestQuery(minimal_changes);
  ConvertResultsToAutocompleteMatches();
}

void SearchProvider::Run() {
  // Start a new request with the current input.
  DCHECK(!done_);
  suggest_results_pending_ = 0;
  if (providers_.valid_suggest_for_keyword_provider()) {
    suggest_results_pending_++;
    keyword_fetcher_.reset(
        CreateSuggestFetcher(kKeywordProviderURLFetcherID,
                             providers_.keyword_provider(),
                             keyword_input_text_));
  }
  if (providers_.valid_suggest_for_default_provider()) {
    suggest_results_pending_++;
    default_fetcher_.reset(
        CreateSuggestFetcher(kDefaultProviderURLFetcherID,
                             providers_.default_provider(), input_.text()));
  }
  // We should only get here if we have a suggest url for the keyword or default
  // providers.
  DCHECK(suggest_results_pending_ > 0);
}

void SearchProvider::Stop() {
  StopHistory();
  StopSuggest();
  done_ = true;
}

void SearchProvider::OnURLFetchComplete(const URLFetcher* source,
                                        const GURL& url,
                                        const URLRequestStatus& status,
                                        int response_code,
                                        const ResponseCookies& cookie,
                                        const std::string& data) {
  DCHECK(!done_);
  suggest_results_pending_--;
  DCHECK(suggest_results_pending_ >= 0);  // Should never go negative.
  const net::HttpResponseHeaders* const response_headers =
      source->response_headers();
  std::string json_data(data);
  // JSON is supposed to be UTF-8, but some suggest service providers send JSON
  // files in non-UTF-8 encodings.  The actual encoding is usually specified in
  // the Content-Type header field.
  if (response_headers) {
    std::string charset;
    if (response_headers->GetCharset(&charset)) {
      std::wstring wide_data;
      // TODO(jungshik): Switch to CodePageToUTF8 after it's added.
      if (CodepageToWide(data, charset.c_str(),
                         OnStringUtilConversionError::FAIL, &wide_data))
        json_data = WideToUTF8(wide_data);
    }
  }

  bool is_keyword_results = (source == keyword_fetcher_.get());
  SuggestResults* suggest_results = is_keyword_results ?
      &keyword_suggest_results_ : &default_suggest_results_;

  if (status.is_success() && response_code == 200) {
    JSONStringValueSerializer deserializer(json_data);
    deserializer.set_allow_trailing_comma(true);
    scoped_ptr<Value> root_val(deserializer.Deserialize(NULL));
    const std::wstring& input_text =
        is_keyword_results ? keyword_input_text_ : input_.text();
    have_suggest_results_ =
        root_val.get() &&
        ParseSuggestResults(root_val.get(), is_keyword_results, input_text,
                            suggest_results);
  }

  ConvertResultsToAutocompleteMatches();
  listener_->OnProviderUpdate(!suggest_results->empty());
}

void SearchProvider::StartOrStopHistoryQuery(bool minimal_changes) {
  // For the minimal_changes case, if we finished the previous query and still
  // have its results, or are allowed to keep running it, just do that, rather
  // than starting a new query.
  if (minimal_changes &&
      (have_history_results_ || (!done_ && !input_.synchronous_only())))
    return;

  // We can't keep running any previous query, so halt it.
  StopHistory();

  // We can't start a new query if we're only allowed synchronous results.
  if (input_.synchronous_only())
    return;

  // Request history for both the keyword and default provider.
  if (providers_.valid_keyword_provider()) {
    ScheduleHistoryQuery(providers_.keyword_provider().id(),
                         keyword_input_text_);
  }
  if (providers_.valid_default_provider()) {
    ScheduleHistoryQuery(providers_.default_provider().id(),
                         input_.text());
  }
}

void SearchProvider::StartOrStopSuggestQuery(bool minimal_changes) {
  // Don't send any queries to the server until some time has elapsed after
  // the last keypress, to avoid flooding the server with requests we are
  // likely to end up throwing away anyway.
  static const int kQueryDelayMs = 200;

  if (!IsQuerySuitableForSuggest()) {
    StopSuggest();
    return;
  }

  // For the minimal_changes case, if we finished the previous query and still
  // have its results, or are allowed to keep running it, just do that, rather
  // than starting a new query.
  if (minimal_changes &&
      (have_suggest_results_ || (!done_ && !input_.synchronous_only())))
    return;

  // We can't keep running any previous query, so halt it.
  StopSuggest();

  // We can't start a new query if we're only allowed synchronous results.
  if (input_.synchronous_only())
    return;

  // We'll have at least one pending fetch. Set it to 1 now, but the value is
  // correctly set in Run. As Run isn't invoked immediately we need to set this
  // now, else we won't think we're waiting on results from the server when we
  // really are.
  suggest_results_pending_ = 1;

  // Kick off a timer that will start the URL fetch if it completes before
  // the user types another character.
  int delay = query_suggest_immediately_ ? 0 : kQueryDelayMs;
  timer_.Start(TimeDelta::FromMilliseconds(delay), this, &SearchProvider::Run);
}

bool SearchProvider::IsQuerySuitableForSuggest() const {
  // Don't run Suggest when off the record, the engine doesn't support it, or
  // the user has disabled it.
  if (profile_->IsOffTheRecord() ||
      (!providers_.valid_suggest_for_keyword_provider() &&
       !providers_.valid_suggest_for_default_provider()) ||
      !profile_->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled))
    return false;

  // If the input type is URL, we take extra care so that private data in URL
  // isn't sent to the server.
  if (input_.type() == AutocompleteInput::URL) {
    // Don't query the server for URLs that aren't http/https/ftp.  Sending
    // things like file: and data: is both a waste of time and a disclosure of
    // potentially private, local data.
    if ((input_.scheme() != L"http") && (input_.scheme() != L"https") &&
        (input_.scheme() != L"ftp"))
      return false;

    // Don't leak private data in URL
    const url_parse::Parsed& parts = input_.parts();

    // Don't send URLs with usernames, queries or refs.  Some of these are
    // private, and the Suggest server is unlikely to have any useful results
    // for any of them.
    // Password is optional and may be omitted.  Checking username is
    // sufficient.
    if (parts.username.is_nonempty() || parts.query.is_nonempty() ||
        parts.ref.is_nonempty())
      return false;
    // Don't send anything for https except hostname and port number.
    // Hostname and port number are OK because they are visible when TCP
    // connection is established and the Suggest server may provide some
    // useful completed URL.
    if (input_.scheme() == L"https" && parts.path.is_nonempty())
      return false;
  }

  return true;
}

void SearchProvider::StopHistory() {
  history_request_consumer_.CancelAllRequests();
  history_request_pending_ = false;
  keyword_history_results_.clear();
  default_history_results_.clear();
  have_history_results_ = false;
}

void SearchProvider::StopSuggest() {
  suggest_results_pending_ = 0;
  timer_.Stop();
  // Stop any in-progress URL fetches.
  keyword_fetcher_.reset();
  default_fetcher_.reset();
  keyword_suggest_results_.clear();
  default_suggest_results_.clear();
  keyword_navigation_results_.clear();
  default_navigation_results_.clear();
  have_suggest_results_ = false;
}

void SearchProvider::ScheduleHistoryQuery(TemplateURL::IDType search_id,
                                          const std::wstring& text) {
  DCHECK(!text.empty());
  HistoryService* const history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  HistoryService::Handle request_handle =
      history_service->GetMostRecentKeywordSearchTerms(
          search_id, text, static_cast<int>(max_matches()),
          &history_request_consumer_,
          NewCallback(this,
                      &SearchProvider::OnGotMostRecentKeywordSearchTerms));
  history_request_consumer_.SetClientData(history_service, request_handle,
                                          search_id);
  history_request_pending_ = true;
}

void SearchProvider::OnGotMostRecentKeywordSearchTerms(
    CancelableRequestProvider::Handle handle,
    HistoryResults* results) {
  HistoryService* history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  DCHECK(history_service);
  if (providers_.valid_keyword_provider() &&
      (providers_.keyword_provider().id() ==
       history_request_consumer_.GetClientData(history_service, handle))) {
    keyword_history_results_ = *results;
  } else {
    default_history_results_ = *results;
  }

  if (history_request_consumer_.PendingRequestCount() == 1) {
    // Requests are removed AFTER the callback is invoked. If the count == 1,
    // it means no more history requests are pending.
    history_request_pending_ = false;
    have_history_results_ = true;
  }

  ConvertResultsToAutocompleteMatches();
  listener_->OnProviderUpdate(!results->empty());
}

URLFetcher* SearchProvider::CreateSuggestFetcher(int id,
                                                 const TemplateURL& provider,
                                                 const std::wstring& text) {
  const TemplateURLRef* const suggestions_url = provider.suggestions_url();
  DCHECK(suggestions_url->SupportsReplacement());
  URLFetcher* fetcher = URLFetcher::Create(id,
      GURL(WideToUTF8(suggestions_url->ReplaceSearchTerms(
          provider, text, TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
          std::wstring()))),
      URLFetcher::GET, this);
  fetcher->set_request_context(profile_->GetRequestContext());
  fetcher->Start();
  return fetcher;
}

bool SearchProvider::ParseSuggestResults(Value* root_val,
                                         bool is_keyword,
                                         const std::wstring& input_text,
                                         SuggestResults* suggest_results) {
  if (!root_val->IsType(Value::TYPE_LIST))
    return false;
  ListValue* root_list = static_cast<ListValue*>(root_val);

  Value* query_val;
  std::wstring query_str;
  Value* result_val;
  if ((root_list->GetSize() < 2) || !root_list->Get(0, &query_val) ||
      !query_val->GetAsString(&query_str) || (query_str != input_text) ||
      !root_list->Get(1, &result_val) || !result_val->IsType(Value::TYPE_LIST))
    return false;

  ListValue* description_list = NULL;
  if (root_list->GetSize() > 2) {
    // 3rd element: Description list.
    Value* description_val;
    if (root_list->Get(2, &description_val) &&
        description_val->IsType(Value::TYPE_LIST))
      description_list = static_cast<ListValue*>(description_val);
  }

  // We don't care about the query URL list (the fourth element in the
  // response) for now.

  // Parse optional data in the results from the Suggest server if any.
  ListValue* type_list = NULL;
  // 5th argument: Optional key-value pairs.
  // TODO: We may iterate the 5th+ arguments of the root_list if any other
  // optional data are defined.
  if (root_list->GetSize() > 4) {
    Value* optional_val;
    if (root_list->Get(4, &optional_val) &&
        optional_val->IsType(Value::TYPE_DICTIONARY)) {
      DictionaryValue* dict_val = static_cast<DictionaryValue*>(optional_val);

      // Parse Google Suggest specific type extension.
      static const std::wstring kGoogleSuggestType(L"google:suggesttype");
      if (dict_val->HasKey(kGoogleSuggestType))
        dict_val->GetList(kGoogleSuggestType, &type_list);
    }
  }

  ListValue* result_list = static_cast<ListValue*>(result_val);
  for (size_t i = 0; i < result_list->GetSize(); ++i) {
    Value* suggestion_val;
    std::wstring suggestion_str;
    if (!result_list->Get(i, &suggestion_val) ||
        !suggestion_val->GetAsString(&suggestion_str))
      return false;

    Value* type_val;
    std::wstring type_str;
    if (type_list && type_list->Get(i, &type_val) &&
        type_val->GetAsString(&type_str) && (type_str == L"NAVIGATION")) {
      Value* site_val;
      std::wstring site_name;
      NavigationResults& navigation_results =
          is_keyword ? keyword_navigation_results_ :
                       default_navigation_results_;
      if ((navigation_results.size() < max_matches()) &&
          description_list && description_list->Get(i, &site_val) &&
          site_val->IsType(Value::TYPE_STRING) &&
          site_val->GetAsString(&site_name)) {
        // We can't blindly trust the URL coming from the server to be valid.
        GURL result_url =
            GURL(URLFixerUpper::FixupURL(WideToUTF8(suggestion_str),
                                         std::string()));
        if (result_url.is_valid())
          navigation_results.push_back(NavigationResult(result_url, site_name));
      }
    } else {
      // TODO(kochi): Currently we treat a calculator result as a query, but it
      // is better to have better presentation for caluculator results.
      if (suggest_results->size() < max_matches())
        suggest_results->push_back(suggestion_str);
    }
  }

  return true;
}

void SearchProvider::ConvertResultsToAutocompleteMatches() {
  // Convert all the results to matches and add them to a map, so we can keep
  // the most relevant match for each result.
  MatchMap map;
  const Time no_time;
  int did_not_accept_keyword_suggestion = keyword_suggest_results_.empty() ?
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE :
      TemplateURLRef::NO_SUGGESTION_CHOSEN;
  // Keyword what you typed results are handled by the KeywordProvider.

  int did_not_accept_default_suggestion = default_suggest_results_.empty() ?
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE :
        TemplateURLRef::NO_SUGGESTION_CHOSEN;
  if (providers_.valid_default_provider()) {
    AddMatchToMap(input_.text(), CalculateRelevanceForWhatYouTyped(),
                  AutocompleteMatch::SEARCH_WHAT_YOU_TYPED,
                  did_not_accept_default_suggestion, false, &map);
  }

  AddHistoryResultsToMap(keyword_history_results_, true,
                         did_not_accept_keyword_suggestion, &map);
  AddHistoryResultsToMap(default_history_results_, false,
                         did_not_accept_default_suggestion, &map);

  AddSuggestResultsToMap(keyword_suggest_results_, true,
                         did_not_accept_keyword_suggestion, &map);
  AddSuggestResultsToMap(default_suggest_results_, false,
                         did_not_accept_default_suggestion, &map);

  // Now add the most relevant matches from the map to |matches_|.
  matches_.clear();
  for (MatchMap::const_iterator i(map.begin()); i != map.end(); ++i)
    matches_.push_back(i->second);

  AddNavigationResultsToMatches(keyword_navigation_results_, true);
  AddNavigationResultsToMatches(default_navigation_results_, false);

  const size_t max_total_matches = max_matches() + 1;  // 1 for "what you typed"
  std::partial_sort(matches_.begin(),
      matches_.begin() + std::min(max_total_matches, matches_.size()),
      matches_.end(), &AutocompleteMatch::MoreRelevant);
  if (matches_.size() > max_total_matches)
    matches_.erase(matches_.begin() + max_total_matches, matches_.end());

  UpdateStarredStateOfMatches();

  // We're done when both asynchronous subcomponents have finished.  We can't
  // use CancelableRequestConsumer.HasPendingRequests() for history requests
  // here.  A pending request is not cleared until after the completion
  // callback has returned, but we've reached here from inside that callback.
  // HasPendingRequests() would therefore return true, and if this is the last
  // thing left to calculate for this query, we'll never mark the query "done".
  done_ = !history_request_pending_ && !suggest_results_pending_;
}

void SearchProvider::AddNavigationResultsToMatches(
    const NavigationResults& navigation_results,
    bool is_keyword) {
  if (!navigation_results.empty()) {
    // TODO(kochi): http://b/1170574  We add only one results for navigational
    // suggestions. If we can get more useful information about the score,
    // consider adding more results.
    matches_.push_back(
        NavigationToMatch(navigation_results.front(),
                          CalculateRelevanceForNavigation(0, is_keyword),
                          is_keyword));
  }
}

void SearchProvider::AddHistoryResultsToMap(const HistoryResults& results,
                                            bool is_keyword,
                                            int did_not_accept_suggestion,
                                            MatchMap* map) {
  for (HistoryResults::const_iterator i(results.begin()); i != results.end();
       ++i) {
    AddMatchToMap(i->term, CalculateRelevanceForHistory(i->time, is_keyword),
                  AutocompleteMatch::SEARCH_HISTORY, did_not_accept_suggestion,
                  is_keyword, map);
  }
}

void SearchProvider::AddSuggestResultsToMap(
    const SuggestResults& suggest_results,
    bool is_keyword,
    int did_not_accept_suggestion,
    MatchMap* map) {
  for (size_t i = 0; i < suggest_results.size(); ++i) {
    AddMatchToMap(suggest_results[i],
                  CalculateRelevanceForSuggestion(suggest_results, i,
                                                  is_keyword),
                  AutocompleteMatch::SEARCH_SUGGEST,
                  static_cast<int>(i), is_keyword, map);
  }
}

int SearchProvider::CalculateRelevanceForWhatYouTyped() const {
  switch (input_.type()) {
    case AutocompleteInput::UNKNOWN:
      return providers_.valid_keyword_provider() ? 250 : 1300;

    case AutocompleteInput::REQUESTED_URL:
      return providers_.valid_keyword_provider() ? 250 : 1200;

    case AutocompleteInput::URL:
      return providers_.valid_keyword_provider() ? 250 : 850;

    case AutocompleteInput::QUERY:
      return providers_.valid_keyword_provider() ? 250 : 1300;

    case AutocompleteInput::FORCED_QUERY:
      return providers_.valid_keyword_provider() ? 250 : 1500;

    default:
      NOTREACHED();
      return 0;
  }
}

int SearchProvider::CalculateRelevanceForHistory(const Time& time,
                                                 bool is_keyword) const {
  // The relevance of past searches falls off over time.  This curve is chosen
  // so that the relevance of a search 15 minutes ago is discounted about 50
  // points, while the relevance of a search two weeks ago is discounted about
  // 450 points.
  const double elapsed_time = std::max((Time::Now() - time).InSecondsF(), 0.);
  const int score_discount = static_cast<int>(6.5 * pow(elapsed_time, 0.3));

  // Don't let scores go below 0.  Negative relevance scores are meaningful in
  // a different way.
  int base_score;
  bool is_primary = providers_.is_primary_provider(is_keyword);
  switch (input_.type()) {
    case AutocompleteInput::UNKNOWN:
    case AutocompleteInput::REQUESTED_URL:
      base_score = is_primary ? 1050 : 200;
      break;

    case AutocompleteInput::URL:
      base_score = is_primary ? 750 : 200;
      break;

    case AutocompleteInput::QUERY:
    case AutocompleteInput::FORCED_QUERY:
      base_score = is_primary ? 1250 : 200;
      break;

    default:
      NOTREACHED();
      base_score = 0;
      break;
  }
  return std::max(0, base_score - score_discount);
}

int SearchProvider::CalculateRelevanceForSuggestion(
    const SuggestResults& suggest_results,
    size_t suggestion_number,
    bool is_keyword) const {
  DCHECK(suggestion_number < suggest_results.size());
  bool is_primary = providers_.is_primary_provider(is_keyword);
  const int suggestion_value =
      static_cast<int>(suggest_results.size() - 1 - suggestion_number);
  switch (input_.type()) {
    case AutocompleteInput::UNKNOWN:
    case AutocompleteInput::REQUESTED_URL:
      return suggestion_value + (is_primary ? 600 : 100);

    case AutocompleteInput::URL:
      return suggestion_value + (is_primary ? 300 : 100);

    case AutocompleteInput::QUERY:
    case AutocompleteInput::FORCED_QUERY:
      return suggestion_value + (is_primary ? 800 : 100);

    default:
      NOTREACHED();
      return 0;
  }
}

int SearchProvider::CalculateRelevanceForNavigation(
    size_t suggestion_number,
    bool is_keyword) const {
  DCHECK(
      (is_keyword && suggestion_number < keyword_navigation_results_.size()) ||
      (!is_keyword && suggestion_number < default_navigation_results_.size()));
  // TODO(kochi): http://b/784900  Use relevance score from the NavSuggest
  // server if possible.
  bool is_primary = providers_.is_primary_provider(is_keyword);
  switch (input_.type()) {
    case AutocompleteInput::QUERY:
    case AutocompleteInput::FORCED_QUERY:
      return static_cast<int>(suggestion_number) + (is_primary ? 1000 : 150);

    default:
      return static_cast<int>(suggestion_number) + (is_primary ? 800 : 150);
  }
}

void SearchProvider::AddMatchToMap(const std::wstring& query_string,
                                   int relevance,
                                   AutocompleteMatch::Type type,
                                   int accepted_suggestion,
                                   bool is_keyword,
                                   MatchMap* map) {
  const std::wstring& input_text =
      is_keyword ? keyword_input_text_ : input_.text();
  AutocompleteMatch match(this, relevance, false, type);
  std::vector<size_t> content_param_offsets;
  const TemplateURL& provider = is_keyword ? providers_.keyword_provider() :
                                             providers_.default_provider();
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableOmnibox2)) {
    match.contents.assign(l10n_util::GetStringF(
        IDS_AUTOCOMPLETE_SEARCH_CONTENTS,
        provider.short_name(),
        query_string,
        &content_param_offsets));
    if (content_param_offsets.size() == 2) {
      AutocompleteMatch::ClassifyLocationInString(content_param_offsets[1],
                                                  query_string.length(),
                                                  match.contents.length(),
                                                  ACMatchClassification::NONE,
                                                  &match.contents_class);
    } else {
      // |content_param_offsets| should only not be 2 if:
      // (a) A translator screws up
      // (b) The strings have been changed and we haven't been rebuilt properly
      // (c) Some sort of crazy installer error/DLL version mismatch problem
      //     that gets the wrong data out of the locale DLL?
      // While none of these are supposed to happen, we've seen this get hit in
      // the wild, so avoid the vector access in the conditional arm above,
      // which will crash.
      NOTREACHED();
    }
  } else {
    // We do intra-string highlighting for suggestions - the suggested segment
    // will be highlighted, e.g. for input_text = "you" the suggestion may be
    // "youtube", so we'll bold the "tube" section: you*tube*.
    if (input_text != query_string) {
      match.contents.assign(query_string);
      size_t input_position = match.contents.find(input_text);
      if (input_position == std::wstring::npos) {
        // The input text is not a substring of the query string, e.g. input
        // text is "slasdot" and the query string is "slashdot", so we bold the
        // whole thing.
        match.contents_class.push_back(
            ACMatchClassification(0, ACMatchClassification::MATCH));
      } else {
        // TODO(beng): ACMatchClassification::MATCH now seems to just mean
        //             "bold" this. Consider modifying the terminology.
        // We don't iterate over the string here annotating all matches because
        // it looks odd to have every occurrence of a substring that may be as
        // short as a single character highlighted in a query suggestion result,
        // e.g. for input text "s" and query string "southwest airlines", it
        // looks odd if both the first and last s are highlighted.
        if (input_position != 0) {
          match.contents_class.push_back(
              ACMatchClassification(0, ACMatchClassification::NONE));
        }
        match.contents_class.push_back(
            ACMatchClassification(input_position, ACMatchClassification::DIM));
        size_t next_fragment_position = input_position + input_text.length();
        if (next_fragment_position < query_string.length()) {
          match.contents_class.push_back(
              ACMatchClassification(next_fragment_position,
                                    ACMatchClassification::NONE));
        }
      }
    } else {
      // Otherwise, we're dealing with the "default search" result which has no
      // completion, but has the search provider name as the description.
      match.contents.assign(query_string);
      match.contents_class.push_back(
          ACMatchClassification(0, ACMatchClassification::NONE));
      match.description.assign(l10n_util::GetStringF(
          IDS_AUTOCOMPLETE_SEARCH_DESCRIPTION,
          provider.short_name()));
      match.description_class.push_back(
          ACMatchClassification(0, ACMatchClassification::DIM));
    }
  }

  // When the user forced a query, we need to make sure all the fill_into_edit
  // values preserve that property.  Otherwise, if the user starts editing a
  // suggestion, non-Search results will suddenly appear.
  size_t search_start = 0;
  if (input_.type() == AutocompleteInput::FORCED_QUERY) {
    match.fill_into_edit.assign(L"?");
    ++search_start;
  }
  if (is_keyword) {
    match.fill_into_edit.append(providers_.keyword_provider().keyword() + L" ");
    match.template_url = &providers_.keyword_provider();
  }
  match.fill_into_edit.append(query_string);
  // NOTE: All Google suggestions currently start with the original input, but
  // not all Yahoo! suggestions do.
  if (!input_.prevent_inline_autocomplete() &&
      !match.fill_into_edit.compare(search_start, input_text.length(),
                                   input_text))
    match.inline_autocomplete_offset = search_start + input_text.length();

  const TemplateURLRef* const search_url = provider.url();
  DCHECK(search_url->SupportsReplacement());
  match.destination_url =
      GURL(WideToUTF8(search_url->ReplaceSearchTerms(provider, query_string,
                                                     accepted_suggestion,
                                                     input_text)));

  // Search results don't look like URLs.
  match.transition =
      is_keyword ? PageTransition::KEYWORD : PageTransition::GENERATED;

  // Try to add |match| to |map|.  If a match for |query_string| is already in
  // |map|, replace it if |match| is more relevant.
  // NOTE: Keep this ToLower() call in sync with url_database.cc.
  const std::pair<MatchMap::iterator, bool> i = map->insert(
      std::pair<std::wstring, AutocompleteMatch>(
      l10n_util::ToLower(query_string), match));
  // NOTE: We purposefully do a direct relevance comparison here instead of
  // using AutocompleteMatch::MoreRelevant(), so that we'll prefer "items added
  // first" rather than "items alphabetically first" when the scores are equal.
  // The only case this matters is when a user has results with the same score
  // that differ only by capitalization; because the history system returns
  // results sorted by recency, this means we'll pick the most recent such
  // result even if the precision of our relevance score is too low to
  // distinguish the two.
  if (!i.second && (match.relevance > i.first->second.relevance))
    i.first->second = match;
}

AutocompleteMatch SearchProvider::NavigationToMatch(
    const NavigationResult& navigation,
    int relevance,
    bool is_keyword) {
  const std::wstring& input_text =
      is_keyword ? keyword_input_text_ : input_.text();
  AutocompleteMatch match(this, relevance, false,
                          AutocompleteMatch::NAVSUGGEST);
  match.destination_url = navigation.url;
  match.contents = StringForURLDisplay(navigation.url, true);
  if (!url_util::FindAndCompareScheme(WideToUTF8(input_text),
                                      chrome::kHttpScheme, NULL))
    TrimHttpPrefix(&match.contents);
  AutocompleteMatch::ClassifyMatchInString(input_text, match.contents,
                                           ACMatchClassification::URL,
                                           &match.contents_class);

  match.description = navigation.site_name;
  AutocompleteMatch::ClassifyMatchInString(input_text, navigation.site_name,
                                           ACMatchClassification::NONE,
                                           &match.description_class);

  // When the user forced a query, we need to make sure all the fill_into_edit
  // values preserve that property.  Otherwise, if the user starts editing a
  // suggestion, non-Search results will suddenly appear.
  if (input_.type() == AutocompleteInput::FORCED_QUERY)
    match.fill_into_edit.assign(L"?");
  match.fill_into_edit.append(match.contents);
  // TODO(pkasting): http://b/1112879 These should perhaps be
  // inline-autocompletable?

  return match;
}
