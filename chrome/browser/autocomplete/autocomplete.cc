// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete.h"

#include <algorithm>

#include "base/string_util.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/autocomplete/history_contents_provider.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/history_tab_ui.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon_ip.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"

#include "generated_resources.h"

using base::TimeDelta;

// AutocompleteInput ----------------------------------------------------------

AutocompleteInput::AutocompleteInput(const std::wstring& text,
                                     const std::wstring& desired_tld,
                                     bool prevent_inline_autocomplete,
                                     bool prefer_keyword,
                                     bool synchronous_only)
    : desired_tld_(desired_tld),
      prevent_inline_autocomplete_(prevent_inline_autocomplete),
      prefer_keyword_(prefer_keyword),
      synchronous_only_(synchronous_only) {
  // Trim whitespace from edges of input; don't inline autocomplete if there
  // was trailing whitespace.
  if (TrimWhitespace(text, TRIM_ALL, &text_) & TRIM_TRAILING)
    prevent_inline_autocomplete_ = true;

  type_ = Parse(text_, desired_tld, &parts_, &scheme_);

  if (type_ == INVALID)
    return;

  if (type_ == FORCED_QUERY && text_[0] == L'?')
    text_.erase(0, 1);
}

// static
std::string AutocompleteInput::TypeToString(Type type) {
  switch (type) {
    case INVALID:       return "invalid";
    case UNKNOWN:       return "unknown";
    case REQUESTED_URL: return "requested-url";
    case URL:           return "url";
    case QUERY:         return "query";
    case FORCED_QUERY:  return "forced-query";

    default:
      NOTREACHED();
      return std::string();
  }
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
         (prevent_inline_autocomplete_ == other.prevent_inline_autocomplete_) &&
         (prefer_keyword_ == other.prefer_keyword_) &&
         (synchronous_only_ == other.synchronous_only_);
}

void AutocompleteInput::Clear() {
  text_.clear();
  type_ = INVALID;
  parts_ = url_parse::Parsed();
  scheme_.clear();
  desired_tld_.clear();
  prevent_inline_autocomplete_ = false;
  prefer_keyword_ = false;
}

// AutocompleteMatch ----------------------------------------------------------

AutocompleteMatch::AutocompleteMatch(AutocompleteProvider* provider,
                                     int relevance,
                                     bool deletable,
                                     Type type)
    : provider(provider),
      relevance(relevance),
      deletable(deletable),
      inline_autocomplete_offset(std::wstring::npos),
      transition(PageTransition::TYPED),
      is_history_what_you_typed_match(false),
      type(type),
      template_url(NULL),
      starred(false) {
}

// static
std::string AutocompleteMatch::TypeToString(Type type) {
  switch (type) {
    case URL_WHAT_YOU_TYPED:    return "url-what-you-typed";
    case HISTORY_URL:           return "history-url";
    case HISTORY_TITLE:         return "history-title";
    case HISTORY_BODY:          return "history-body";
    case HISTORY_KEYWORD:       return "history-keyword";
    case NAVSUGGEST:            return "navsuggest";
    case SEARCH_WHAT_YOU_TYPED: return "search-what-you-typed";
    case SEARCH_HISTORY:        return "search-history";
    case SEARCH_SUGGEST:        return "search-suggest";
    case SEARCH_OTHER_ENGINE:   return "search-other-engine";
    case OPEN_HISTORY_PAGE:     return "open-history-page";

    default:
      NOTREACHED();
      return std::string();
  }
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
  BookmarkModel* bookmark_model = profile_->GetBookmarkModel();
  if (!bookmark_model || !bookmark_model->IsLoaded())
    return;

  for (ACMatches::iterator i = matches_.begin(); i != matches_.end(); ++i)
    i->starred = bookmark_model->IsBookmarked(GURL(i->destination_url));
}

// AutocompleteResult ---------------------------------------------------------

// static
size_t AutocompleteResult::max_matches_ = 6;

void AutocompleteResult::Selection::Clear() {
  destination_url = GURL();
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
  std::copy(matches.begin(), matches.end(), std::back_inserter(matches_));
  default_match_ = end();
}

void AutocompleteResult::AddMatch(const AutocompleteMatch& match) {
  DCHECK(default_match_ != end());
  ACMatches::iterator insertion_point =
      std::upper_bound(begin(), end(), match, &AutocompleteMatch::MoreRelevant);
  ACMatches::iterator::difference_type default_offset =
      default_match_ - begin();
  if ((insertion_point - begin()) <= default_offset)
    ++default_offset;
  matches_.insert(insertion_point, match);
  default_match_ = begin() + default_offset;
}

void AutocompleteResult::SortAndCull() {
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
    matches_.erase(matches_.begin() + max_matches(), matches_.end());
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
  default_match_ = begin();
}

GURL AutocompleteResult::GetAlternateNavURL(
    const AutocompleteInput& input,
    const_iterator match) const {
  if (((input.type() == AutocompleteInput::UNKNOWN) ||
       (input.type() == AutocompleteInput::REQUESTED_URL)) &&
      (match->transition != PageTransition::TYPED)) {
    for (const_iterator i(begin()); i != end(); ++i) {
      if (i->is_history_what_you_typed_match) {
        return (i->destination_url == match->destination_url) ?
            GURL() : i->destination_url;
      }
    }
  }
  return GURL();
}

#ifndef NDEBUG
void AutocompleteResult::Validate() const {
  for (const_iterator i(begin()); i != end(); ++i)
    i->Validate();
}
#endif

// AutocompleteController -----------------------------------------------------

const int AutocompleteController::kNoItemSelected = -1;

namespace {
// The amount of time we'll wait after a provider returns before updating,
// in order to coalesce results.
const int kResultCoalesceMs = 100;

// The maximum time we'll allow the results to go without updating to the
// latest set.
const int kResultUpdateMaxDelayMs = 300;
};

AutocompleteController::AutocompleteController(Profile* profile)
    : update_pending_(false),
      done_(true) {
  providers_.push_back(new SearchProvider(this, profile));
  providers_.push_back(new HistoryURLProvider(this, profile));
  providers_.push_back(new KeywordProvider(this, profile));
  history_contents_provider_ = new HistoryContentsProvider(this, profile);
  providers_.push_back(history_contents_provider_);
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

void AutocompleteController::Start(const std::wstring& text,
                                   const std::wstring& desired_tld,
                                   bool prevent_inline_autocomplete,
                                   bool prefer_keyword,
                                   bool synchronous_only) {
  // See if we can avoid rerunning autocomplete when the query hasn't changed
  // much.  When the user presses or releases the ctrl key, the desired_tld
  // changes, and when the user finishes an IME composition, inline autocomplete
  // may no longer be prevented.  In both these cases the text itself hasn't
  // changed since the last query, and some providers can do much less work (and
  // get results back more quickly).  Taking advantage of this reduces flicker.
  const bool minimal_changes = (input_.text() == text) &&
      (input_.synchronous_only() == synchronous_only);
  input_ = AutocompleteInput(text, desired_tld, prevent_inline_autocomplete,
                             prefer_keyword, synchronous_only);

  // If we're starting a brand new query, stop caring about any old query.
  if (!minimal_changes && !done_) {
    update_pending_ = false;
    coalesce_timer_.Stop();
  }

  // Start the new query.
  for (ACProviders::iterator i(providers_.begin()); i != providers_.end();
       ++i) {
    (*i)->Start(input_, minimal_changes);
    if (synchronous_only)
      DCHECK((*i)->done());
  }
  UpdateLatestResult(true);
}

void AutocompleteController::Stop(bool clear_result) {
  for (ACProviders::const_iterator i(providers_.begin());
       i != providers_.end(); ++i) {
    if (!(*i)->done())
      (*i)->Stop();
  }

  done_ = true;
  update_pending_ = false;
  if (clear_result)
    result_.Reset();
  latest_result_.CopyFrom(result_);  // Not strictly necessary, but keeps
                                     // internal state consistent.
  coalesce_timer_.Stop();
  max_delay_timer_.Stop();
}

void AutocompleteController::DeleteMatch(const AutocompleteMatch& match) {
  DCHECK(match.deletable);
  match.provider->DeleteMatch(match);  // This will synchronously call back to
                                       // OnProviderUpdate().

  // Notify observers of this change immediately, so the UI feels responsive to
  // the user's action.
  if (update_pending_)
    CommitResult();
}

void AutocompleteController::OnProviderUpdate(bool updated_matches) {
  DCHECK(!input_.synchronous_only());

  if (updated_matches) {
    UpdateLatestResult(false);
    return;
  }

  done_ = true;
  for (ACProviders::const_iterator i(providers_.begin());
       i != providers_.end(); ++i) {
    if (!(*i)->done()) {
      done_ = false;
      return;
    }
  }
  // In theory we could call Stop() instead of CommitLatestResults() here if we
  // knew we'd already called CommitLatestResults() at least once for this
  // query.  In practice, our observers don't do enough work responding to the
  // updates here for the potentially-extra notification to matter.
  CommitResult();
}

void AutocompleteController::UpdateLatestResult(bool is_synchronous_pass) {
  // Add all providers' results.
  latest_result_.Reset();
  done_ = true;
  for (ACProviders::const_iterator i(providers_.begin());
       i != providers_.end(); ++i) {
    latest_result_.AppendMatches((*i)->matches());
    if (!(*i)->done())
      done_ = false;
  }

  // Sort the matches and trim to a small number of "best" matches.
  latest_result_.SortAndCull();

  if (history_contents_provider_)
    AddHistoryContentsShortcut();

#ifndef NDEBUG
  latest_result_.Validate();
#endif

  if (is_synchronous_pass) {
    if (!max_delay_timer_.IsRunning()) {
      max_delay_timer_.Start(
          TimeDelta::FromMilliseconds(kResultUpdateMaxDelayMs),
          this, &AutocompleteController::CommitResult);
    }

    result_.CopyFrom(latest_result_);
    NotificationService::current()->Notify(
        NotificationType::AUTOCOMPLETE_CONTROLLER_SYNCHRONOUS_MATCHES_AVAILABLE,
        Source<AutocompleteController>(this), NotificationService::NoDetails());
  }

  if (done_) {
    CommitResult();
  } else if (!update_pending_) {
    // Coalesce the results for the next kPopupCoalesceMs milliseconds.
    update_pending_ = true;
    coalesce_timer_.Stop();
    coalesce_timer_.Start(TimeDelta::FromMilliseconds(kResultCoalesceMs), this,
                          &AutocompleteController::CommitResult);
  }
}

void AutocompleteController::CommitResult() {
  // The max update interval timer either needs to be reset (if more updates
  // are to come) or stopped (when we're done with the query).  The coalesce
  // timer should always just be stopped.
  update_pending_ = false;
  coalesce_timer_.Stop();
  if (done_)
    max_delay_timer_.Stop();
  else
    max_delay_timer_.Reset();

  result_.CopyFrom(latest_result_);
  NotificationService::current()->Notify(
      NotificationType::AUTOCOMPLETE_CONTROLLER_RESULT_UPDATED,
      Source<AutocompleteController>(this), NotificationService::NoDetails());
}

ACMatches AutocompleteController::GetMatchesNotInLatestResult(
    const AutocompleteProvider* provider) const {
  DCHECK(provider);

  // Determine the set of destination URLs.
  std::set<GURL> destination_urls;
  for (AutocompleteResult::const_iterator i(latest_result_.begin());
       i != latest_result_.end(); ++i)
    destination_urls.insert(i->destination_url);

  ACMatches matches;
  const ACMatches& provider_matches = provider->matches();
  for (ACMatches::const_iterator i = provider_matches.begin();
       i != provider_matches.end(); ++i) {
    if (destination_urls.find(i->destination_url) == destination_urls.end())
      matches.push_back(*i);
  }

  return matches;
}

void AutocompleteController::AddHistoryContentsShortcut() {
  DCHECK(history_contents_provider_);
  // Only check the history contents provider if the history contents provider
  // is done and has matches.
  if (!history_contents_provider_->done() ||
      !history_contents_provider_->db_match_count()) {
    return;
  }

  if ((history_contents_provider_->db_match_count() <=
          (latest_result_.size() + 1)) ||
      (history_contents_provider_->db_match_count() == 1)) {
    // We only want to add a shortcut if we're not already showing the matches.
    ACMatches matches(GetMatchesNotInLatestResult(history_contents_provider_));
    if (matches.empty())
      return;
    if (matches.size() == 1) {
      // Only one match not shown, add it. The relevance may be negative,
      // which means we need to negate it to get the true relevance.
      AutocompleteMatch& match = matches.front();
      if (match.relevance < 0)
        match.relevance = -match.relevance;
      latest_result_.AddMatch(match);
      return;
    } // else, fall through and add item.
  }

  AutocompleteMatch match(NULL, 0, false, AutocompleteMatch::OPEN_HISTORY_PAGE);
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
      HistoryTabUI::GetHistoryURLWithSearchText(input_.text());
  match.transition = PageTransition::AUTO_BOOKMARK;
  match.provider = history_contents_provider_;
  latest_result_.AddMatch(match);
}
