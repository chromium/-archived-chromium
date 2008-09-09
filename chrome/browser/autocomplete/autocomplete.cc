// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/autocomplete/autocomplete.h"

#include "base/string_util.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/autocomplete/history_contents_provider.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/bookmarks/bookmark_bar_model.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/history_tab_ui.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/url_fixer_upper.h"
#include "chrome/common/gfx/url_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/url_canon_ip.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"

#include "generated_resources.h"


// AutocompleteInput ----------------------------------------------------------

AutocompleteInput::AutocompleteInput(const std::wstring& text,
                                     const std::wstring& desired_tld,
                                     bool prevent_inline_autocomplete)
    : desired_tld_(desired_tld),
      prevent_inline_autocomplete_(prevent_inline_autocomplete) {
  // Trim whitespace from edges of input; don't inline autocomplete if there
  // was trailing whitespace.
  if (TrimWhitespace(text, TRIM_ALL, &text_) & TRIM_TRAILING)
    prevent_inline_autocomplete_ = true;

  url_parse::Parsed parts;
  type_ = Parse(text_, desired_tld, &parts, &scheme_);

  if (type_ == INVALID)
    return;

  if (type_ == FORCED_QUERY && text_[0] == L'?')
    text_.erase(0, 1);
}

//static
AutocompleteInput::Type AutocompleteInput::Parse(const std::wstring& text,
                                                 const std::wstring& desired_tld,
                                                 url_parse::Parsed* parts,
                                                 std::wstring* scheme) {
  DCHECK(parts);

  const size_t first_non_white = text.find_first_not_of(kWhitespaceWide, 0);
  if (first_non_white == std::wstring::npos)
    return INVALID;  // All whitespace.

  if (text.at(first_non_white) == L'?') {
    // If the first non-whitespace character is a '?', we magically treat this
    // as a query.
    return FORCED_QUERY;
  }

  // Ask our parsing back-end to help us understand what the user typed.  We
  // use the URLFixerUpper here because we want to be smart about what we
  // consider a scheme.  For example, we shouldn't consider www.google.com:80
  // to have a scheme.
  const std::wstring parsed_scheme(URLFixerUpper::SegmentURL(text, parts));
  if (scheme)
    *scheme = parsed_scheme;

  if (parsed_scheme == L"file") {
    // A user might or might not type a scheme when entering a file URL.
    return URL;
  }

  // If the user typed a scheme, determine our available actions based on that.
  if (parts->scheme.is_valid()) {
    // See if we know how to handle the URL internally.
    if (URLRequest::IsHandledProtocol(WideToASCII(parsed_scheme)))
      return URL;

    // There are also some schemes that we convert to other things before they
    // reach the renderer or else the renderer handles internally without
    // reaching the URLRequest logic.  We thus won't catch these above, but we
    // should still claim to handle them.
    if ((parsed_scheme == L"view-source") || (parsed_scheme == L"javascript") ||
        (parsed_scheme == L"data"))
      return URL;

    // Finally, check and see if the user has explicitly opened this scheme as
    // a URL before.  We need to do this last because some schemes may be in
    // here as "blocked" (e.g. "javascript") because we don't want pages to open
    // them, but users still can.
    switch (ExternalProtocolHandler::GetBlockState(parsed_scheme)) {
      case ExternalProtocolHandler::DONT_BLOCK:
        return URL;

      case ExternalProtocolHandler::BLOCK:
        // If we don't want the user to open the URL, don't let it be navigated
        // to at all.
        return QUERY;

      default:
        // We don't know about this scheme.  It's likely to be a search operator
        // like "site:" or "link:".  We classify it as UNKNOWN so the user has
        // the option of treating it as a URL if we're wrong.
        // Note that SegmentURL() is smart so we aren't tricked by "c:\foo" or
        // "www.example.com:81" in this case.
        return UNKNOWN;
    }
  }

  // The user didn't type a scheme.  Assume that this is either an HTTP URL or
  // not a URL at all; try to determine which.

  // It's not clear that we can reach here with an empty "host" (maybe on some
  // kinds of garbage input?), but if we did, it couldn't be a URL.
  if (!parts->host.is_nonempty())
    return QUERY;
  // (We use the registry length later below but ask for it here so we can check
  // the host's validity at this point.)
  const std::wstring host(text.substr(parts->host.begin, parts->host.len));
  const size_t registry_length =
      net::RegistryControlledDomainService::GetRegistryLength(host, false);
  if (registry_length == std::wstring::npos)
    return QUERY;  // It's not clear to me that we can reach this...

  // A space in the "host" means this is a query.  (Technically, IE and GURL
  // allow hostnames with spaces for wierd intranet machines, but it's supposed
  // to be illegal and I'm not worried about users trying to type these in.)
  if (host.find(' ') != std::wstring::npos)
    return QUERY;

  // Presence of a password/port mean this is almost certainly a URL.  We don't
  // treat usernames (without passwords) as indicating a URL, because this could
  // be an email address like "user@mail.com" which is more likely a search than
  // an HTTP auth login attempt.
  if (parts->password.is_nonempty() || parts->port.is_nonempty())
    return URL;

  // See if the host is an IP address.
  bool is_ip_address;
  net::CanonicalizeHost(host, &is_ip_address);
  if (is_ip_address) {
    // If the user originally typed a host that looks like an IP address (a
    // dotted quad), they probably want to open it.  If the original input was
    // something else (like a single number), they probably wanted to search for
    // it.  This is true even if the URL appears to have a path: "1.2/45" is
    // more likely a search (for the answer to a math problem) than a URL.
    url_parse::Component components[4];
    const bool found_ipv4 =
        url_canon::FindIPv4Components(text.c_str(), parts->host, components);
    DCHECK(found_ipv4);
    for (size_t i = 0; i < arraysize(components); ++i) {
      if (!components[i].is_nonempty())
        return UNKNOWN;
    }
    return URL;
  }

  // The host doesn't look like a number, so see if the user's given us a path.
  if (parts->path.is_nonempty()) {
    // Most inputs with paths are URLs, even ones without known registries (e.g.
    // intranet URLs).  However, if there's no known registry, and the path has
    // a space, this is more likely a query with a slash in the first term (e.g.
    // "ps/2 games") than a URL.  We can still open URLs with spaces in the path
    // by escaping the space, and we will still inline autocomplete them if
    // users have typed them in the past, but we default to searching since
    // that's the common case.
    return ((registry_length == 0) &&
            (text.substr(parts->path.begin, parts->path.len).find(' ') !=
                std::wstring::npos)) ? UNKNOWN : URL;
  }

  // If we reach here with a username, our input looks like "user@host"; this is
  // the case mentioned above, where we think this is more likely an email
  // address than an HTTP auth attempt, so search for it.
  if (parts->username.is_nonempty())
    return UNKNOWN;

  // We have a bare host string.  See if it has a known TLD.  If so, it's
  // probably a URL.
  if (registry_length != 0)
    return URL;

  // No TLD that we know about.  This could be:
  // * A string that the user wishes to add a desired_tld to to get a URL.  If
  //   we reach this point, we know there's no known TLD on the string, so the
  //   fixup code will be willing to add one; thus this is a URL.
  // * A single word "foo"; possibly an intranet site, but more likely a search.
  //   This is ideally an UNKNOWN, and we can let the Alternate Nav URL code
  //   catch our mistakes.
  // * A URL with a valid TLD we don't know about yet.  If e.g. a registrar adds
  //   "xxx" as a TLD, then until we add it to our data file, Chrome won't know
  //   "foo.xxx" is a real URL.  So ideally this is a URL, but we can't really
  //   distinguish this case from:
  // * A "URL-like" string that's not really a URL (like
  //   "browser.tabs.closeButtons" or "java.awt.event.*").  This is ideally a
  //   QUERY.  Since the above case and this one are indistinguishable, and this
  //   case is likely to be much more common, just say these are both UNKNOWN,
  //   which should default to the right thing and let users correct us on a
  //   case-by-case basis.
  return desired_tld.empty() ? UNKNOWN : REQUESTED_URL;
}

bool AutocompleteInput::Equals(const AutocompleteInput& other) const {
  return (text_ == other.text_) &&
         (type_ == other.type_) &&
         (desired_tld_ == other.desired_tld_) &&
         (scheme_ == other.scheme_) &&
         (prevent_inline_autocomplete_ == other.prevent_inline_autocomplete_);
}

void AutocompleteInput::Clear() {
  text_.clear();
  type_ = INVALID;
  scheme_.clear();
  desired_tld_.clear();
  prevent_inline_autocomplete_ = false;
}

// AutocompleteMatch ----------------------------------------------------------

AutocompleteMatch::AutocompleteMatch()
    : provider(NULL),
      relevance(0),
      deletable(false),
      inline_autocomplete_offset(std::wstring::npos),
      transition(PageTransition::TYPED),
      is_history_what_you_typed_match(false),
      type(URL),
      template_url(NULL),
      starred(false) {
}

AutocompleteMatch::AutocompleteMatch(AutocompleteProvider* provider,
                                     int relevance,
                                     bool deletable)
    : provider(provider),
      relevance(relevance),
      deletable(deletable),
      inline_autocomplete_offset(std::wstring::npos),
      transition(PageTransition::TYPED),
      is_history_what_you_typed_match(false),
      type(URL),
      template_url(NULL),
      starred(false) {
}

// static
bool AutocompleteMatch::MoreRelevant(const AutocompleteMatch& elem1,
                                     const AutocompleteMatch& elem2) {
  // For equal-relevance matches, we sort alphabetically, so that providers
  // who return multiple elements at the same priority get a "stable" sort
  // across multiple updates.
  if (elem1.relevance == elem2.relevance)
    return elem1.contents > elem2.contents;

  // A negative relevance indicates the real relevance can be determined by
  // negating the value. If both relevances are negative, negate the result
  // so that we end up with positive relevances, then negative relevances with
  // the negative relevances sorted by absolute values.
  const bool result = elem1.relevance > elem2.relevance;
  return (elem1.relevance < 0 && elem2.relevance < 0) ? !result : result;
}

// static
bool AutocompleteMatch::DestinationSortFunc(const AutocompleteMatch& elem1,
                                            const AutocompleteMatch& elem2) {
  // Sort identical destination_urls together.  Place the most relevant matches
  // first, so that when we call std::unique(), these are the ones that get
  // preserved.
  return (elem1.destination_url != elem2.destination_url) ?
      (elem1.destination_url < elem2.destination_url) :
      MoreRelevant(elem1, elem2);
}

// static
bool AutocompleteMatch::DestinationsEqual(const AutocompleteMatch& elem1,
                                          const AutocompleteMatch& elem2) {
  return elem1.destination_url == elem2.destination_url;
}

// static
void AutocompleteMatch::ClassifyMatchInString(
    const std::wstring& find_text,
    const std::wstring& text,
    int style,
    ACMatchClassifications* classification) {
  ClassifyLocationInString(text.find(find_text), find_text.length(),
                           text.length(), style, classification);
}

void AutocompleteMatch::ClassifyLocationInString(
    size_t match_location,
    size_t match_length,
    size_t overall_length,
    int style,
    ACMatchClassifications* classification) {
  // Classifying an empty match makes no sense and will lead to validation
  // errors later.
  DCHECK(match_length > 0);

  classification->clear();

  // Don't classify anything about an empty string
  // (AutocompleteMatch::Validate() checks this).
  if (overall_length == 0)
    return;

  // Mark pre-match portion of string (if any).
  if (match_location != 0) {
    classification->push_back(ACMatchClassification(0, style));
  }

  // Mark matching portion of string.
  if (match_location == std::wstring::npos) {
    // No match, above classification will suffice for whole string.
    return;
  }
  classification->push_back(ACMatchClassification(match_location,
      (style | ACMatchClassification::MATCH) & ~ACMatchClassification::DIM));

  // Mark post-match portion of string (if any).
  const size_t after_match(match_location + match_length);
  if (after_match < overall_length) {
    classification->push_back(ACMatchClassification(after_match, style));
  }
}

#ifndef NDEBUG
void AutocompleteMatch::Validate() const {
  ValidateClassifications(contents, contents_class);
  ValidateClassifications(description, description_class);
}

void AutocompleteMatch::ValidateClassifications(
    const std::wstring& text,
    const ACMatchClassifications& classifications) const {
  if (text.empty()) {
    DCHECK(classifications.size() == 0);
    return;
  }

  // The classifications should always cover the whole string.
  DCHECK(classifications.size() > 0) << "No classification for text";
  DCHECK(classifications[0].offset == 0) << "Classification misses beginning";
  if (classifications.size() == 1)
    return;

  // The classifications should always be sorted.
  size_t last_offset = classifications[0].offset;
  for (ACMatchClassifications::const_iterator i(classifications.begin() + 1);
       i != classifications.end(); ++i) {
    DCHECK(i->offset > last_offset) << "Classification unsorted";
    DCHECK(i->offset < text.length()) << "Classification out of bounds";
    last_offset = i->offset;
  }
}
#endif

// AutocompleteProvider -------------------------------------------------------

// static
size_t AutocompleteProvider::max_matches_ = 3;

AutocompleteProvider::~AutocompleteProvider() {
  Stop();
}

void AutocompleteProvider::SetProfile(Profile* profile) {
  DCHECK(profile);
  Stop();  // It makes no sense to continue running a query from an old profile.
  profile_ = profile;
}

std::wstring AutocompleteProvider::StringForURLDisplay(
    const GURL& url,
    bool check_accept_lang) {
  return gfx::ElideUrl(url, ChromeFont(), 0, check_accept_lang && profile_ ?
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages) :
      std::wstring());
}

void AutocompleteProvider::UpdateStarredStateOfMatches() {
  if (matches_.empty())
    return;

  if (!profile_)
    return;
  BookmarkBarModel* bookmark_bar_model = profile_->GetBookmarkBarModel();
  if (!bookmark_bar_model || !bookmark_bar_model->IsLoaded())
    return;

  for (ACMatches::iterator i = matches_.begin(); i != matches_.end(); ++i)
    i->starred = bookmark_bar_model->IsBookmarked(GURL(i->destination_url));
}

// AutocompleteResult ---------------------------------------------------------

// static
size_t AutocompleteResult::max_matches_ = 6;

void AutocompleteResult::Selection::Clear() {
  destination_url.clear();
  provider_affinity = NULL;
  is_history_what_you_typed_match = false;
}

AutocompleteResult::AutocompleteResult() {
  // Reserve space for the max number of matches we'll show. The +1 accounts
  // for the history shortcut match as it isn't included in max_matches.
  matches_.reserve(max_matches() + 1);

  // It's probably safe to do this in the initializer list, but there's little
  // penalty to doing it here and it ensures our object is fully constructed
  // before calling member functions.
  default_match_ = end();
}

void AutocompleteResult::CopyFrom(const AutocompleteResult& rhs) {
  if (this == &rhs)
    return;

  matches_ = rhs.matches_;
  // Careful!  You can't just copy iterators from another container, you have to
  // reconstruct them.
  default_match_ = (rhs.default_match_ == rhs.end()) ?
      end() : (begin() + (rhs.default_match_ - rhs.begin()));
}

void AutocompleteResult::AppendMatches(const ACMatches& matches) {
  default_match_ = end();
  std::copy(matches.begin(), matches.end(), std::back_inserter(matches_));
}

void AutocompleteResult::AddMatch(const AutocompleteMatch& match) {
  default_match_ = end();
  matches_.insert(std::upper_bound(matches_.begin(), matches_.end(), match,
                                   &AutocompleteMatch::MoreRelevant), match);
}

void AutocompleteResult::SortAndCull() {
  default_match_ = end();

  // Remove duplicates.
  std::sort(matches_.begin(), matches_.end(),
            &AutocompleteMatch::DestinationSortFunc);
  matches_.erase(std::unique(matches_.begin(), matches_.end(),
                             &AutocompleteMatch::DestinationsEqual),
                 matches_.end());

  // Find the top max_matches.
  if (matches_.size() > max_matches()) {
    std::partial_sort(matches_.begin(), matches_.begin() + max_matches(),
                      matches_.end(), &AutocompleteMatch::MoreRelevant);
    matches_.resize(max_matches());
  }

  // HistoryContentsProvider use a negative relevance as a way to avoid
  // starving out other provider results, yet we may end up using the result. To
  // make sure such results are sorted correctly we search for all
  // relevances < 0 and negate them. If we change our relevance algorithm to
  // properly mix different providers results, this can go away.
  for (ACMatches::iterator i = matches_.begin(); i != matches_.end(); ++i) {
    if (i->relevance < 0)
      i->relevance = -i->relevance;
  }

  // Now put the final result set in order.
  std::sort(matches_.begin(), matches_.end(), &AutocompleteMatch::MoreRelevant);
}

bool AutocompleteResult::SetDefaultMatch(const Selection& selection) {
  default_match_ = end();

  // Look for the best match.
  for (const_iterator i(begin()); i != end(); ++i) {
    // If we have an exact match, return immediately.
    if (selection.is_history_what_you_typed_match ?
        i->is_history_what_you_typed_match :
        (!selection.destination_url.empty() &&
         (i->destination_url == selection.destination_url))) {
      default_match_ = i;
      return true;
    }

    // Otherwise, see if this match is closer to the desired selection than the
    // existing one.
    if (default_match_ == end()) {
      // No match at all yet, pick the first one we see.
      default_match_ = i;
    } else if (selection.provider_affinity == NULL) {
      // No provider desired, choose solely based on relevance.
      if (AutocompleteMatch::MoreRelevant(*i, *default_match_))
        default_match_ = i;
    } else {
      // Desired provider trumps any undesired provider; otherwise choose based
      // on relevance.
      const bool providers_match =
          (i->provider == selection.provider_affinity);
      const bool default_provider_doesnt_match =
          (default_match_->provider != selection.provider_affinity);
      if ((providers_match && default_provider_doesnt_match) ||
          ((providers_match || default_provider_doesnt_match) &&
           AutocompleteMatch::MoreRelevant(*i, *default_match_)))
        default_match_ = i;
    }
  }
  return false;
}

std::wstring AutocompleteResult::GetAlternateNavURL(
    const AutocompleteInput& input,
    const_iterator match) const {
  if (((input.type() == AutocompleteInput::UNKNOWN) ||
       (input.type() == AutocompleteInput::REQUESTED_URL)) &&
      (match->transition != PageTransition::TYPED)) {
    for (const_iterator i(begin()); i != end(); ++i) {
      if (i->is_history_what_you_typed_match) {
        return (i->destination_url == match->destination_url) ?
            std::wstring() : i->destination_url;
      }
    }
  }
  return std::wstring();
}

#ifndef NDEBUG
void AutocompleteResult::Validate() const {
  for (const_iterator i(begin()); i != end(); ++i)
    i->Validate();
}
#endif

// AutocompleteController -----------------------------------------------------

const int AutocompleteController::kNoItemSelected = -1;

AutocompleteController::AutocompleteController(ACControllerListener* listener,
                                               Profile* profile)
    : listener_(listener) {
  providers_.push_back(new SearchProvider(this, profile));
  providers_.push_back(new HistoryURLProvider(this, profile));
  keyword_provider_ = new KeywordProvider(this, profile);
  providers_.push_back(keyword_provider_);
  if (listener) {
    // These providers are async-only, so there's no need to create them when
    // we'll only be doing synchronous queries.
    history_contents_provider_ = new HistoryContentsProvider(this, profile);
    providers_.push_back(history_contents_provider_);
  } else {
    history_contents_provider_ = NULL;
  }
  for (ACProviders::iterator i(providers_.begin()); i != providers_.end(); ++i)
    (*i)->AddRef();
}

AutocompleteController::~AutocompleteController() {
  for (ACProviders::iterator i(providers_.begin()); i != providers_.end(); ++i)
    (*i)->Release();

  providers_.clear();  // Not really necessary.
}

void AutocompleteController::SetProfile(Profile* profile) {
  for (ACProviders::iterator i(providers_.begin()); i != providers_.end(); ++i)
    (*i)->SetProfile(profile);
}

bool AutocompleteController::Start(const AutocompleteInput& input,
                                   bool minimal_changes,
                                   bool synchronous_only) {
  input_ = input;
  for (ACProviders::iterator i(providers_.begin()); i != providers_.end();
       ++i) {
    (*i)->Start(input, minimal_changes, synchronous_only);
    if (synchronous_only)
      DCHECK((*i)->done());
  }

  return QueryComplete();
}

void AutocompleteController::Stop() const {
  for (ACProviders::const_iterator i(providers_.begin());
       i != providers_.end(); ++i) {
    if (!(*i)->done())
      (*i)->Stop();
  }
}

void AutocompleteController::OnProviderUpdate(bool updated_matches) {
  // Notify listener when something has changed.
  if (!listener_) {
    NOTREACHED();  // This should never be called for synchronous queries, and
                   // since |listener_| is NULL, the owner of the controller
                   // should only be running synchronous queries.
    return;        // But, this isn't fatal, so don't crash.
  }
  const bool query_complete = QueryComplete();
  if (updated_matches || query_complete)
    listener_->OnAutocompleteUpdate(updated_matches, query_complete);
}

void AutocompleteController::GetResult(AutocompleteResult* result) {
  // Add all providers' results.
  result->Reset();
  for (ACProviders::const_iterator i(providers_.begin());
       i != providers_.end(); ++i)
    result->AppendMatches((*i)->matches());

  // Sort the matches and trim to a small number of "best" matches.
  result->SortAndCull();

  if (history_contents_provider_)
    AddHistoryContentsShortcut(result);

#ifndef NDEBUG
  result->Validate();
#endif
}

bool AutocompleteController::QueryComplete() const {
  for (ACProviders::const_iterator i(providers_.begin());
       i != providers_.end(); ++i) {
    if (!(*i)->done())
      return false;
  }

  return true;
}

size_t AutocompleteController::CountMatchesNotInResult(
    const AutocompleteProvider* provider,
    const AutocompleteResult* result,
    AutocompleteMatch* first_match) {
  DCHECK(provider && result && first_match);

  // Determine the set of destination URLs.
  std::set<std::wstring> destination_urls;
  for (AutocompleteResult::const_iterator i(result->begin());
       i != result->end(); ++i)
    destination_urls.insert(i->destination_url);

  const ACMatches& provider_matches = provider->matches();
  bool found_first_unique_match = false;
  size_t showing_count = 0;
  for (ACMatches::const_iterator i = provider_matches.begin();
       i != provider_matches.end(); ++i) {
    if (destination_urls.find(i->destination_url) != destination_urls.end()) {
      showing_count++;
    } else if (!found_first_unique_match) {
      found_first_unique_match = true;
      *first_match = *i;
    }
  }
  return provider_matches.size() - showing_count;
}

void AutocompleteController::AddHistoryContentsShortcut(
    AutocompleteResult* result) {
  DCHECK(result && history_contents_provider_);
  // Only check the history contents provider if the history contents provider
  // is done and has matches.
  if (!history_contents_provider_->done() ||
      !history_contents_provider_->db_match_count()) {
    return;
  }

  if ((history_contents_provider_->db_match_count() <= result->size() + 1) ||
      history_contents_provider_->db_match_count() == 1) {
    // We only want to add a shortcut if we're not already showing the matches.
    AutocompleteMatch first_unique_match;
    size_t matches_not_shown = CountMatchesNotInResult(
        history_contents_provider_, result, &first_unique_match);
    if (matches_not_shown == 0)
      return;
    if (matches_not_shown == 1) {
      // Only one match not shown, add it. The relevance may be negative,
      // which means we need to negate it to get the true relevance.
      if (first_unique_match.relevance < 0)
        first_unique_match.relevance = -first_unique_match.relevance;
      result->AddMatch(first_unique_match);
      return;
    } // else, fall through and add item.
  }

  AutocompleteMatch match(NULL, 0, false);
  match.type = AutocompleteMatch::HISTORY_SEARCH;
  match.fill_into_edit = input_.text();

  // Mark up the text such that the user input text is bold.
  size_t keyword_offset = std::wstring::npos;  // Offset into match.contents.
  if (history_contents_provider_->db_match_count() ==
      history_contents_provider_->kMaxMatchCount) {
    // History contents searcher has maxed out.
    match.contents = l10n_util::GetStringF(IDS_OMNIBOX_RECENT_HISTORY_MANY,
                                           input_.text(),
                                           &keyword_offset);
  } else {
    // We can report exact matches when there aren't too many.
    std::vector<size_t> content_param_offsets;
    match.contents =
        l10n_util::GetStringF(IDS_OMNIBOX_RECENT_HISTORY,
                              FormatNumber(history_contents_provider_->
                                           db_match_count()),
                              input_.text(),
                              &content_param_offsets);

    // content_param_offsets is ordered based on supplied params, we expect
    // that the second one contains the query (first is the number).
    if (content_param_offsets.size() == 2) {
      keyword_offset = content_param_offsets[1];
    } else {
      // See comments on an identical NOTREACHED() in search_provider.cc.
      NOTREACHED();
    }
  }

  // NOTE: This comparison succeeds when keyword_offset == std::wstring::npos.
  if (keyword_offset > 0) {
    match.contents_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
  }
  match.contents_class.push_back(
      ACMatchClassification(keyword_offset, ACMatchClassification::MATCH));
  if (keyword_offset + input_.text().size() < match.contents.size()) {
    match.contents_class.push_back(
        ACMatchClassification(keyword_offset + input_.text().size(),
                              ACMatchClassification::NONE));
  }
  match.destination_url =
      UTF8ToWide(HistoryTabUI::GetHistoryURLWithSearchText(
          input_.text()).spec());
  match.transition = PageTransition::AUTO_BOOKMARK;
  match.provider = history_contents_provider_;
  result->AddMatch(match);
}

