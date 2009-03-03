// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/search_provider.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google_util.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/l10n_util.h"
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

void SearchProvider::Start(const AutocompleteInput& input,
                           bool minimal_changes) {
  matches_.clear();

  // Can't return search/suggest results for bogus input or without a profile.
  if (!profile_ || (input.type() == AutocompleteInput::INVALID)) {
    Stop();
    return;
  }

  // Can't search without a default provider.
  const TemplateURL* const current_default_provider =
      profile_->GetTemplateURLModel()->GetDefaultSearchProvider();
  // TODO(pkasting): http://b/1155786  Eventually we should not need all these
  // checks.
  if (!current_default_provider || !current_default_provider->url() ||
      !current_default_provider->url()->SupportsReplacement()) {
    Stop();
    return;
  }

  // If we're still running an old query but have since changed the query text
  // or the default provider, abort the query.
  if (!done_ && (!minimal_changes ||
                 (last_default_provider_ != current_default_provider)))
    Stop();

  // TODO(pkasting): http://b/1162970  We shouldn't need to structure-copy this.
  // Nor should we need |last_default_provider_| just to know whether the
  // provider changed.
  default_provider_ = *current_default_provider;
  last_default_provider_ = current_default_provider;

  if (input.text().empty()) {
    // User typed "?" alone.  Give them a placeholder result indicating what
    // this syntax does.
    AutocompleteMatch match(this, 0, false,
                            AutocompleteMatch::SEARCH_WHAT_YOU_TYPED);
    static const std::wstring kNoQueryInput(
        l10n_util::GetString(IDS_AUTOCOMPLETE_NO_QUERY));
    match.contents.assign(l10n_util::GetStringF(
        IDS_AUTOCOMPLETE_SEARCH_CONTENTS, default_provider_.short_name(),
        kNoQueryInput));
    match.contents_class.push_back(
        ACMatchClassification(0, ACMatchClassification::DIM));
    matches_.push_back(match);
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
  const TemplateURLRef* const suggestions_url =
      default_provider_.suggestions_url();
  DCHECK(suggestions_url->SupportsReplacement());
  fetcher_.reset(new URLFetcher(suggestions_url->ReplaceSearchTerms(
      default_provider_, input_.text(),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()),
      URLFetcher::GET, this));
  fetcher_->set_request_context(profile_->GetRequestContext());
  fetcher_->Start();
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
  suggest_results_pending_ = false;
  suggest_results_.clear();
  navigation_results_.clear();
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

  if (status.is_success() && response_code == 200) {
    JSONStringValueSerializer deserializer(json_data);
    deserializer.set_allow_trailing_comma(true);
    scoped_ptr<Value> root_val(deserializer.Deserialize(NULL));
    have_suggest_results_ =
        root_val.get() && ParseSuggestResults(root_val.get());
  }

  ConvertResultsToAutocompleteMatches();
  listener_->OnProviderUpdate(!suggest_results_.empty());
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

  // Start the history query.
  HistoryService* const history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  history_service->GetMostRecentKeywordSearchTerms(default_provider_.id(),
      input_.text(), static_cast<int>(max_matches()),
      &history_request_consumer_,
      NewCallback(this, &SearchProvider::OnGotMostRecentKeywordSearchTerms));
  history_request_pending_ = true;
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

  // Kick off a timer that will start the URL fetch if it completes before
  // the user types another character.
  suggest_results_pending_ = true;

  timer_.Stop();
  timer_.Start(TimeDelta::FromMilliseconds(kQueryDelayMs), this,
               &SearchProvider::Run);
}

bool SearchProvider::IsQuerySuitableForSuggest() const {
  // Don't run Suggest when off the record, the engine doesn't support it, or
  // the user has disabled it.
  if (profile_->IsOffTheRecord() ||
      !default_provider_.suggestions_url() ||
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
  history_results_.clear();
  have_history_results_ = false;
}

void SearchProvider::StopSuggest() {
  suggest_results_pending_ = false;
  timer_.Stop();
  fetcher_.reset();  // Stop any in-progress URL fetch.
  suggest_results_.clear();
  have_suggest_results_ = false;
}

void SearchProvider::OnGotMostRecentKeywordSearchTerms(
    CancelableRequestProvider::Handle handle,
    HistoryResults* results) {
  history_request_pending_ = false;
  have_history_results_ = true;
  history_results_ = *results;
  ConvertResultsToAutocompleteMatches();
  listener_->OnProviderUpdate(!history_results_.empty());
}

bool SearchProvider::ParseSuggestResults(Value* root_val) {
  if (!root_val->IsType(Value::TYPE_LIST))
    return false;
  ListValue* root_list = static_cast<ListValue*>(root_val);

  Value* query_val;
  std::wstring query_str;
  Value* result_val;
  if ((root_list->GetSize() < 2) || !root_list->Get(0, &query_val) ||
      !query_val->GetAsString(&query_str) || (query_str != input_.text()) ||
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
      static const string16 kGoogleSuggestType(LIT16("google:suggesttype"));
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
      if ((navigation_results_.size() < max_matches()) &&
          description_list && description_list->Get(i, &site_val) &&
          site_val->IsType(Value::TYPE_STRING) &&
          site_val->GetAsString(&site_name)) {
        // We can't blindly trust the URL coming from the server to be valid.
        GURL result_url =
            GURL(URLFixerUpper::FixupURL(WideToUTF8(suggestion_str),
                                         std::string()));
        if (result_url.is_valid()) {
          navigation_results_.push_back(NavigationResult(result_url,
                                                         site_name));
        }
      }
    } else {
      // TODO(kochi): Currently we treat a calculator result as a query, but it
      // is better to have better presentation for caluculator results.
      if (suggest_results_.size() < max_matches())
        suggest_results_.push_back(suggestion_str);
    }
  }

  return true;
}

void SearchProvider::ConvertResultsToAutocompleteMatches() {
  // Convert all the results to matches and add them to a map, so we can keep
  // the most relevant match for each result.
  MatchMap map;
  const int did_not_accept_suggestion = suggest_results_.empty() ?
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE :
      TemplateURLRef::NO_SUGGESTION_CHOSEN;
  const Time no_time;
  AddMatchToMap(input_.text(), CalculateRelevanceForWhatYouTyped(),
                AutocompleteMatch::SEARCH_WHAT_YOU_TYPED,
                did_not_accept_suggestion, &map);

  for (HistoryResults::const_iterator i(history_results_.begin());
       i != history_results_.end(); ++i) {
    AddMatchToMap(i->term, CalculateRelevanceForHistory(i->time),
                  AutocompleteMatch::SEARCH_HISTORY, did_not_accept_suggestion,
                  &map);
  }

  for (size_t i = 0; i < suggest_results_.size(); ++i) {
    AddMatchToMap(suggest_results_[i], CalculateRelevanceForSuggestion(i),
                  AutocompleteMatch::SEARCH_SUGGEST,
                  static_cast<int>(i), &map);
  }

  // Now add the most relevant matches from the map to |matches_|.
  matches_.clear();
  for (MatchMap::const_iterator i(map.begin()); i != map.end(); ++i)
    matches_.push_back(i->second);

  if (!navigation_results_.empty()) {
    // TODO(kochi): http://b/1170574  We add only one results for navigational
    // suggestions. If we can get more useful information about the score,
    // consider adding more results.
    matches_.push_back(NavigationToMatch(navigation_results_.front(),
                                         CalculateRelevanceForNavigation(0)));
  }

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
  done_ = !history_request_pending_ &&
          !suggest_results_pending_;
}

int SearchProvider::CalculateRelevanceForWhatYouTyped() const {
  switch (input_.type()) {
    case AutocompleteInput::UNKNOWN:
      return 1300;

    case AutocompleteInput::REQUESTED_URL:
      return 1200;

    case AutocompleteInput::URL:
      return 850;

    case AutocompleteInput::QUERY:
      return 1300;

    case AutocompleteInput::FORCED_QUERY:
      return 1500;

    default:
      NOTREACHED();
      return 0;
  }
}

int SearchProvider::CalculateRelevanceForHistory(const Time& time) const {
  // The relevance of past searches falls off over time.  This curve is chosen
  // so that the relevance of a search 15 minutes ago is discounted about 50
  // points, while the relevance of a search two weeks ago is discounted about
  // 450 points.
  const double elapsed_time = std::max((Time::Now() - time).InSecondsF(), 0.);
  const int score_discount = static_cast<int>(6.5 * pow(elapsed_time, 0.3));

  // Don't let scores go below 0.  Negative relevance scores are meaningful in
  // a different way.
  int base_score;
  switch (input_.type()) {
    case AutocompleteInput::UNKNOWN:
    case AutocompleteInput::REQUESTED_URL:
      base_score = 1050;
      break;

    case AutocompleteInput::URL:
      base_score = 750;
      break;

    case AutocompleteInput::QUERY:
    case AutocompleteInput::FORCED_QUERY:
      base_score = 1250;
      break;

    default:
      NOTREACHED();
      base_score = 0;
      break;
  }
  return std::max(0, base_score - score_discount);
}

int SearchProvider::CalculateRelevanceForSuggestion(
    size_t suggestion_number) const {
  DCHECK(suggestion_number < suggest_results_.size());
  const int suggestion_value =
      static_cast<int>(suggest_results_.size() - 1 - suggestion_number);
  switch (input_.type()) {
    case AutocompleteInput::UNKNOWN:
    case AutocompleteInput::REQUESTED_URL:
      return 600 + suggestion_value;

    case AutocompleteInput::URL:
      return 300 + suggestion_value;

    case AutocompleteInput::QUERY:
    case AutocompleteInput::FORCED_QUERY:
      return 800 + suggestion_value;

    default:
      NOTREACHED();
      return 0;
  }
}

int SearchProvider::CalculateRelevanceForNavigation(
    size_t suggestion_number) const {
  DCHECK(suggestion_number < navigation_results_.size());
  // TODO(kochi): http://b/784900  Use relevance score from the NavSuggest
  // server if possible.
  switch (input_.type()) {
    case AutocompleteInput::QUERY:
    case AutocompleteInput::FORCED_QUERY:
      return 1000 + static_cast<int>(suggestion_number);

    default:
      return 800 + static_cast<int>(suggestion_number);
  }
}

void SearchProvider::AddMatchToMap(const std::wstring& query_string,
                                   int relevance,
                                   AutocompleteMatch::Type type,
                                   int accepted_suggestion,
                                   MatchMap* map) {
  AutocompleteMatch match(this, relevance, false, type);
  std::vector<size_t> content_param_offsets;
  match.contents.assign(l10n_util::GetStringF(IDS_AUTOCOMPLETE_SEARCH_CONTENTS,
                                              default_provider_.short_name(),
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
    // (c) Some sort of crazy installer error/DLL version mismatch problem that
    //     gets the wrong data out of the locale DLL?
    // While none of these are supposed to happen, we've seen this get hit in
    // the wild, so avoid the vector access in the conditional arm above, which
    // will crash.
    NOTREACHED();
  }

  // When the user forced a query, we need to make sure all the fill_into_edit
  // values preserve that property.  Otherwise, if the user starts editing a
  // suggestion, non-Search results will suddenly appear.
  size_t search_start = 0;
  if (input_.type() == AutocompleteInput::FORCED_QUERY) {
    match.fill_into_edit.assign(L"?");
    ++search_start;
  }
  match.fill_into_edit.append(query_string);
  // NOTE: All Google suggestions currently start with the original input, but
  // not all Yahoo! suggestions do.
  if (!input_.prevent_inline_autocomplete() &&
      !match.fill_into_edit.compare(search_start, input_.text().length(),
                                   input_.text()))
    match.inline_autocomplete_offset = search_start + input_.text().length();

  const TemplateURLRef* const search_url = default_provider_.url();
  DCHECK(search_url->SupportsReplacement());
  match.destination_url = search_url->ReplaceSearchTerms(default_provider_,
                                                         query_string,
                                                         accepted_suggestion,
                                                         input_.text());

  // Search results don't look like URLs.
  match.transition = PageTransition::GENERATED;

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
    int relevance) {
  AutocompleteMatch match(this, relevance, false,
                          AutocompleteMatch::NAVSUGGEST);
  match.destination_url = navigation.url;
  match.contents = StringForURLDisplay(navigation.url, true);
  // TODO(kochi): Consider moving HistoryURLProvider::TrimHttpPrefix() to some
  // public utility function.
  if (!url_util::FindAndCompareScheme(WideToUTF8(input_.text()),
                                      "http", NULL))
    TrimHttpPrefix(&match.contents);
  AutocompleteMatch::ClassifyMatchInString(input_.text(), match.contents,
                                           ACMatchClassification::URL,
                                           &match.contents_class);

  match.description = navigation.site_name;
  AutocompleteMatch::ClassifyMatchInString(input_.text(), navigation.site_name,
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

// TODO(kochi): This is duplicate from HistoryURLProvider.
// static
size_t SearchProvider::TrimHttpPrefix(std::wstring* url) {
  url_parse::Component scheme;
  if (!url_util::FindAndCompareScheme(WideToUTF8(*url), chrome::kHttpScheme,
                                      &scheme))
    return 0;  // Not "http".

  // Erase scheme plus up to two slashes.
  size_t prefix_len = scheme.end() + 1;  // "http:"
  const size_t after_slashes = std::min(url->length(),
                                        static_cast<size_t>(scheme.end() + 3));
  while ((prefix_len < after_slashes) && ((*url)[prefix_len] == L'/'))
    ++prefix_len;
  if (prefix_len == url->length())
    url->clear();
  else
    url->erase(url->begin(), url->begin() + prefix_len);
  return prefix_len;
}
