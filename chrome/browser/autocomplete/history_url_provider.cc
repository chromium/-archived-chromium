// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_url_provider.h"

#include <algorithm>

#include "base/histogram.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/sqlite_utils.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/url_util.h"
#include "net/base/net_util.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

HistoryURLProviderParams::HistoryURLProviderParams(
    const AutocompleteInput& input,
    bool trim_http,
    const ACMatches& matches,
    const std::wstring& languages)
    : message_loop(MessageLoop::current()),
      input(input),
      trim_http(trim_http),
      cancel(false),
      matches(matches),
      languages(languages) {
}

void HistoryURLProvider::Start(const AutocompleteInput& input,
                               bool minimal_changes) {
  // NOTE: We could try hard to do less work in the |minimal_changes| case
  // here; some clever caching would let us reuse the raw matches from the
  // history DB without re-querying.  However, we'd still have to go back to
  // the history thread to mark these up properly, and if pass 2 is currently
  // running, we'd need to wait for it to return to the main thread before
  // doing this (we can't just write new data for it to read due to thread
  // safety issues).  At that point it's just as fast, and easier, to simply
  // re-run the query from scratch and ignore |minimal_changes|.

  // Cancel any in-progress query.
  Stop();

  RunAutocompletePasses(input, true);
}

void HistoryURLProvider::Stop() {
  done_ = true;

  if (params_)
    params_->cancel = true;
}

void HistoryURLProvider::DeleteMatch(const AutocompleteMatch& match) {
  // Delete the match from the history DB.
  HistoryService* history_service =
      profile_ ? profile_->GetHistoryService(Profile::EXPLICIT_ACCESS) :
      history_service_;
  GURL selected_url(match.destination_url);
  if (!history_service || !selected_url.is_valid()) {
    NOTREACHED() << "Can't delete requested URL";
    return;
  }
  history_service->DeleteURL(selected_url);

  // Delete the match from the current set of matches.
  bool found = false;
  for (ACMatches::iterator i(matches_.begin()); i != matches_.end(); ++i) {
    if (i->destination_url == match.destination_url) {
      found = true;
      if (i->is_history_what_you_typed_match) {
        // We can't get rid of the What You Typed match, but we can make it
        // look like it has no backing data.
        i->deletable = false;
        i->description.clear();
        i->description_class.clear();
      } else {
        matches_.erase(i);
      }
      break;
    }
  }
  DCHECK(found) << "Asked to delete a URL that isn't in our set of matches";
  listener_->OnProviderUpdate(true);

  // Cancel any current pass 2 and rerun it, so we get correct history data.
  if (!done_) {
    // Copy params_->input to avoid a race condition where params_ gets deleted
    // out from under us on the other thread after we set params_->cancel here.
    AutocompleteInput input(params_->input);
    params_->cancel = true;
    RunAutocompletePasses(input, false);
  }
}

// Called on the history thread.
void HistoryURLProvider::ExecuteWithDB(history::HistoryBackend* backend,
                                       history::URLDatabase* db,
                                       HistoryURLProviderParams* params) {
  // We may get called with a NULL database if it couldn't be properly
  // initialized.  In this case we just say the query is complete.
  if (db && !params->cancel) {
    TimeTicks beginning_time = TimeTicks::Now();

    DoAutocomplete(backend, db, params);

    HISTOGRAM_TIMES(L"Autocomplete.HistoryAsyncQueryTime",
                    TimeTicks::Now() - beginning_time);
  }

  // Return the results (if any) to the main thread.
  params->message_loop->PostTask(FROM_HERE, NewRunnableMethod(
      this, &HistoryURLProvider::QueryComplete, params));
}

// Used by both autocomplete passes, and therefore called on multiple different
// threads (though not simultaneously).
void HistoryURLProvider::DoAutocomplete(history::HistoryBackend* backend,
                                        history::URLDatabase* db,
                                        HistoryURLProviderParams* params) {
  // Get the matching URLs from the DB
  typedef std::vector<history::URLRow> URLRowVector;
  URLRowVector url_matches;
  HistoryMatches history_matches;
  for (Prefixes::const_iterator i(prefixes_.begin()); i != prefixes_.end();
       ++i) {
    if (params->cancel)
      return;  // canceled in the middle of a query, give up
    // We only need max_matches results in the end, but before we get there we
    // need to promote lower-quality matches that are prefixes of
    // higher-quality matches, and remove lower-quality redirects.  So we ask
    // for more results than we need, of every prefix type, in hopes this will
    // give us far more than enough to work with.  CullRedirects() will then
    // reduce the list to the best max_matches results.
    db->AutocompleteForPrefix(i->prefix + params->input.text(),
                              max_matches() * 2, &url_matches);
    for (URLRowVector::const_iterator j(url_matches.begin());
         j != url_matches.end(); ++j) {
      const Prefix* best_prefix = BestPrefix(j->url(), std::wstring());
      DCHECK(best_prefix != NULL);
      history_matches.push_back(HistoryMatch(*j, i->prefix.length(),
          !i->num_components,
          i->num_components >= best_prefix->num_components));
    }
  }

  // Create sorted list of suggestions.
  CullPoorMatches(&history_matches);
  SortMatches(&history_matches);
  PromoteOrCreateShorterSuggestion(db, *params, &history_matches);

  // Try to promote a match as an exact/inline autocomplete match.  This also
  // moves it to the front of |history_matches|, so skip over it when
  // converting the rest of the matches.  We want to provide up to max_matches
  // results plus the What You Typed result.
  size_t first_match = 1;
  size_t exact_suggestion = 0;
  if (!params->matches.empty() &&
      FixupExactSuggestion(db, params, &history_matches))
    exact_suggestion = 1;
  else if (params->input.prevent_inline_autocomplete() ||
           history_matches.empty() ||
           !PromoteMatchForInlineAutocomplete(params, history_matches.front()))
    first_match = 0;

  // This is the end of the synchronous pass.
  if (!backend)
    return;

  // Remove redirects and trim list to size.
  CullRedirects(backend, &history_matches, max_matches() + exact_suggestion);

  // Convert the history matches to autocomplete matches.
  for (size_t i = first_match; i < history_matches.size(); ++i) {
    const HistoryMatch& match = history_matches[i];
    DCHECK(!exact_suggestion ||
           (match.url_info.url() !=
            GURL(params->matches.front().destination_url)));
    params->matches.push_back(HistoryMatchToACMatch(params, match, NORMAL,
        history_matches.size() - 1 - i));
  }
}

// Called on the main thread when the query is complete.
void HistoryURLProvider::QueryComplete(
    HistoryURLProviderParams* params_gets_deleted) {
  // Ensure |params_gets_deleted| gets deleted on exit.
  scoped_ptr<HistoryURLProviderParams> params(params_gets_deleted);

  // If the user hasn't already started another query, clear our member pointer
  // so we can't write into deleted memory.
  if (params_ == params_gets_deleted)
    params_ = NULL;

  // Don't send responses for queries that have been canceled.
  if (params->cancel)
    return;  // Already set done_ when we canceled, no need to set it again.

  done_ = true;
  matches_.swap(params->matches);
  UpdateStarredStateOfMatches();
  listener_->OnProviderUpdate(true);
}

void HistoryURLProvider::SuggestExactInput(const AutocompleteInput& input,
                                           bool trim_http) {
  AutocompleteMatch match(this,
      CalculateRelevance(input.type(), WHAT_YOU_TYPED, 0), false,
      AutocompleteMatch::URL_WHAT_YOU_TYPED);

  // Try to canonicalize the URL.  If this fails, don't create a What You Typed
  // suggestion, since it can't be navigated to.  We also need this so other
  // history suggestions don't duplicate the same effective URL as this.
  // TODO(brettw) make autocomplete use GURL!
  GURL canonicalized_url(URLFixerUpper::FixupURL(input.text(),
                                                 input.desired_tld()));
  if (!canonicalized_url.is_valid() ||
      (canonicalized_url.IsStandard() &&
       !canonicalized_url.SchemeIsFile() && canonicalized_url.host().empty()))
    return;
  match.destination_url = canonicalized_url;
  match.fill_into_edit = StringForURLDisplay(canonicalized_url, false);
  // NOTE: Don't set match.input_location (to allow inline autocompletion)
  // here, it's surprising and annoying.
  // Trim off "http://" if the user didn't type it.
  const size_t offset = trim_http ? TrimHttpPrefix(&match.fill_into_edit) : 0;

  // Try to highlight "innermost" match location.  If we fix up "w" into
  // "www.w.com", we want to highlight the fifth character, not the first.
  // This relies on match.destination_url being the non-prefix-trimmed version
  // of match.contents.
  match.contents = match.fill_into_edit;
  const Prefix* best_prefix = BestPrefix(match.destination_url, input.text());
  // Because of the vagaries of GURL, it's possible for match.destination_url
  // to not contain the user's input at all.  In this case don't mark anything
  // as a match.
  const size_t match_location = (best_prefix == NULL) ?
      std::wstring::npos : best_prefix->prefix.length() - offset;
  AutocompleteMatch::ClassifyLocationInString(match_location,
                                              input.text().length(),
                                              match.contents.length(),
                                              ACMatchClassification::URL,
                                              &match.contents_class);

  match.is_history_what_you_typed_match = true;
  matches_.push_back(match);
}

bool HistoryURLProvider::FixupExactSuggestion(history::URLDatabase* db,
                                              HistoryURLProviderParams* params,
                                              HistoryMatches* matches) const {
  DCHECK(!params->matches.empty());

  history::URLRow info;
  AutocompleteMatch& match = params->matches.front();

  // Tricky corner case: The user has visited intranet site "foo", but not
  // internet site "www.foo.com".  He types in foo (getting an exact match),
  // then tries to hit ctrl-enter.  When pressing ctrl, the what-you-typed
  // match ("www.foo.com") doesn't show up in history, and thus doesn't get a
  // promoted relevance, but a different match from the input ("foo") does, and
  // gets promoted for inline autocomplete.  Thus instead of getting
  // "www.foo.com", the user still gets "foo" (and, before hitting enter,
  // probably gets an odd-looking inline autocomplete of "/").
  //
  // We detect this crazy case as follows:
  // * If the what-you-typed match is not in the history DB,
  // * and the user has specified a TLD,
  // * and the input _without_ the TLD _is_ in the history DB,
  // * ...then just before pressing "ctrl" the best match we supplied was the
  //   what-you-typed match, so stick with it by promoting this.
  if (!db->GetRowForURL(match.destination_url, &info)) {
    if (params->input.desired_tld().empty())
      return false;
    // This code should match what SuggestExactInput() would do with no
    // desired_tld().
    // TODO(brettw) make autocomplete use GURL!
    GURL destination_url(URLFixerUpper::FixupURL(params->input.text(),
                                                 std::wstring()));
    if (!db->GetRowForURL(destination_url, &info))
      return false;
  } else {
    // We have data for this match, use it.
    match.deletable = true;
    match.description = info.title();
    AutocompleteMatch::ClassifyMatchInString(params->input.text(),
                                             info.title(),
                                             ACMatchClassification::NONE,
                                             &match.description_class);
  }

  // Promote as an exact match.
  match.relevance = CalculateRelevance(params->input.type(),
                                       INLINE_AUTOCOMPLETE, 0);

  // Put it on the front of the HistoryMatches for redirect culling.
  EnsureMatchPresent(info, std::wstring::npos, false, matches, true);
  return true;
}

bool HistoryURLProvider::PromoteMatchForInlineAutocomplete(
    HistoryURLProviderParams* params,
    const HistoryMatch& match) {
  // Promote the first match if it's been typed at least n times, where n == 1
  // for "simple" (host-only) URLs and n == 2 for others.  We set a higher bar
  // for these long URLs because it's less likely that users will want to visit
  // them again.  Even though we don't increment the typed_count for pasted-in
  // URLs, if the user manually edits the URL or types some long thing in by
  // hand, we wouldn't want to immediately start autocompleting it.
  if (!match.url_info.typed_count() ||
      ((match.url_info.typed_count() == 1) &&
       !IsHostOnly(match.url_info.url())))
    return false;

  params->matches.push_back(HistoryMatchToACMatch(params, match,
                                                  INLINE_AUTOCOMPLETE, 0));
  return true;
}

// static
std::wstring HistoryURLProvider::FixupUserInput(const std::wstring& input) {
  // Fixup and canonicalize user input.
  const GURL canonical_gurl(URLFixerUpper::FixupURL(input, std::wstring()));
  std::wstring output(UTF8ToWide(canonical_gurl.possibly_invalid_spec()));
  if (output.empty())
    return input;  // This probably won't happen, but there are no guarantees.

  // Don't prepend a scheme when the user didn't have one.  Since the fixer
  // upper only prepends the "http" scheme, that's all we need to check for.
  url_parse::Component scheme;
  if (canonical_gurl.SchemeIs("http") &&
      !url_util::FindAndCompareScheme(input, "http", &scheme))
    TrimHttpPrefix(&output);

  // Make the number of trailing slashes on the output exactly match the input.
  // Examples of why not doing this would matter:
  // * The user types "a" and has this fixed up to "a/".  Now no other sites
  //   beginning with "a" will match.
  // * The user types "file:" and has this fixed up to "file://".  Now inline
  //   autocomplete will append too few slashes, resulting in e.g. "file:/b..."
  //   instead of "file:///b..."
  // * The user types "http:/" and has this fixed up to "http:".  Now inline
  //   autocomplete will append too many slashes, resulting in e.g.
  //   "http:///c..." instead of "http://c...".
  // NOTE: We do this after calling TrimHttpPrefix() since that can strip
  // trailing slashes (if the scheme is the only thing in the input).  It's not
  // clear that the result of fixup really matters in this case, but there's no
  // harm in making sure.
  const size_t last_input_nonslash = input.find_last_not_of(L"/\\");
  const size_t num_input_slashes = (last_input_nonslash == std::wstring::npos) ?
      input.length() : (input.length() - 1 - last_input_nonslash);
  const size_t last_output_nonslash = output.find_last_not_of(L"/\\");
  const size_t num_output_slashes =
      (last_output_nonslash == std::wstring::npos) ?
      output.length() : (output.length() - 1 - last_output_nonslash);
  if (num_output_slashes < num_input_slashes)
    output.append(num_input_slashes - num_output_slashes, '/');
  else if (num_output_slashes > num_input_slashes)
    output.erase(output.length() - num_output_slashes + num_input_slashes);

  return output;
}

// static
size_t HistoryURLProvider::TrimHttpPrefix(std::wstring* url) {
  url_parse::Component scheme;
  if (!url_util::FindAndCompareScheme(*url, "http", &scheme))
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

// static
bool HistoryURLProvider::IsHostOnly(const GURL& url) {
  DCHECK(url.is_valid());
  return (!url.has_path() || (url.path() == "/")) && !url.has_query() &&
      !url.has_ref();
}

// static
bool HistoryURLProvider::CompareHistoryMatch(const HistoryMatch& a,
                                             const HistoryMatch& b) {
  // A URL that has been typed at all is better than one that has never been
  // typed.  (Note "!"s on each side)
  if (!a.url_info.typed_count() != !b.url_info.typed_count())
    return a.url_info.typed_count() > b.url_info.typed_count();

  // Innermost matches (matches after any scheme or "www.") are better than
  // non-innermost matches.
  if (a.innermost_match != b.innermost_match)
    return a.innermost_match;

  // URLs that have been typed more often are better.
  if (a.url_info.typed_count() != b.url_info.typed_count())
    return a.url_info.typed_count() > b.url_info.typed_count();

  // For URLs that have each been typed once, a host (alone) is better than a
  // page inside.
  if (a.url_info.typed_count() == 1) {
    const bool a_is_host_only = IsHostOnly(a.url_info.url());
    if (a_is_host_only != IsHostOnly(b.url_info.url()))
      return a_is_host_only;
  }

  // URLs that have been visited more often are better.
  if (a.url_info.visit_count() != b.url_info.visit_count())
    return a.url_info.visit_count() > b.url_info.visit_count();

  // URLs that have been visited more recently are better.
  return a.url_info.last_visit() > b.url_info.last_visit();
}

// static
HistoryURLProvider::Prefixes HistoryURLProvider::GetPrefixes() {
  // We'll complete text following these prefixes.
  // NOTE: There's no requirement that these be in any particular order.
  Prefixes prefixes;
  prefixes.push_back(Prefix(L"https://www.", 2));
  prefixes.push_back(Prefix(L"http://www.", 2));
  prefixes.push_back(Prefix(L"ftp://ftp.", 2));
  prefixes.push_back(Prefix(L"ftp://www.", 2));
  prefixes.push_back(Prefix(L"https://", 1));
  prefixes.push_back(Prefix(L"http://", 1));
  prefixes.push_back(Prefix(L"ftp://", 1));
  prefixes.push_back(Prefix(L"", 0));  // Catches within-scheme matches as well
  return prefixes;
}

// static
int HistoryURLProvider::CalculateRelevance(AutocompleteInput::Type input_type,
                                           MatchType match_type,
                                           size_t match_number) {
  switch (match_type) {
    case INLINE_AUTOCOMPLETE:
      return 1400;

    case WHAT_YOU_TYPED:
      return (input_type == AutocompleteInput::REQUESTED_URL) ? 1300 : 1200;

    default:
      return 900 + static_cast<int>(match_number);
  }
}

// static
GURL HistoryURLProvider::ConvertToHostOnly(const HistoryMatch& match,
                                           const std::wstring& input) {
  // See if we should try to do host-only suggestions for this URL. Nonstandard
  // schemes means there's no authority section, so suggesting the host name
  // is useless. File URLs are standard, but host suggestion is not useful for
  // them either.
  const GURL& url = match.url_info.url();
  if (!url.is_valid() || !url.IsStandard() || url.SchemeIsFile())
    return GURL();

  // Transform to a host-only match.  Bail if the host no longer matches the
  // user input (e.g. because the user typed more than just a host).
  GURL host = url.GetWithEmptyPath();
  if ((host.spec().length() < (match.input_location + input.length())))
    return GURL();  // User typing is longer than this host suggestion.

  const std::wstring spec = UTF8ToWide(host.spec());
  if (spec.compare(match.input_location, input.length(), input))
    return GURL();  // User typing is no longer a prefix.

  return host;
}

// static
void HistoryURLProvider::PromoteOrCreateShorterSuggestion(
    history::URLDatabase* db,
    const HistoryURLProviderParams& params,
    HistoryMatches* matches) {
  if (matches->empty())
    return;  // No matches, nothing to do.

  // Determine the base URL from which to search, and whether that URL could
  // itself be added as a match.  We can add the base iff it's not "effectively
  // the same" as any "what you typed" match.
  const HistoryMatch& match = matches->front();
  GURL search_base = ConvertToHostOnly(match, params.input.text());
  bool can_add_search_base_to_matches = params.matches.empty();
  if (search_base.is_empty()) {
    // Search from what the user typed when we couldn't reduce the best match
    // to a host.  Careful: use a substring of |match| here, rather than the
    // first match in |params|, because they might have different prefixes.  If
    // the user typed "google.com", params.matches will hold
    // "http://google.com/", but |match| might begin with
    // "http://www.google.com/".
    // TODO: this should be cleaned up, and is probably incorrect for IDN.
    std::string new_match = match.url_info.url().possibly_invalid_spec().
        substr(0, match.input_location + params.input.text().length());
    search_base = GURL(new_match);

  } else if (!can_add_search_base_to_matches) {
    can_add_search_base_to_matches =
        (search_base != params.matches.front().destination_url);
  }
  if (search_base == match.url_info.url())
    return;  // Couldn't shorten |match|, so no range of URLs to search over.

  // Search the DB for short URLs between our base and |match|.
  history::URLRow info(search_base);
  bool promote = true;
  // A short URL is only worth suggesting if it's been visited at least a third
  // as often as the longer URL.
  const int min_visit_count = ((match.url_info.visit_count() - 1) / 3) + 1;
  // For stability between the in-memory and on-disk autocomplete passes, when
  // the long URL has been typed before, only suggest shorter URLs that have
  // also been typed.  Otherwise, the on-disk pass could suggest a shorter URL
  // (which hasn't been typed) that the in-memory pass doesn't know about,
  // thereby making the top match, and thus the behavior of inline
  // autocomplete, unstable.
  const int min_typed_count = match.url_info.typed_count() ? 1 : 0;
  if (!db->FindShortestURLFromBase(search_base.possibly_invalid_spec(),
          match.url_info.url().possibly_invalid_spec(), min_visit_count,
          min_typed_count, can_add_search_base_to_matches, &info)) {
    if (!can_add_search_base_to_matches)
      return;  // Couldn't find anything and can't add the search base, bail.

    // Try to get info on the search base itself.  Promote it to the top if the
    // original best match isn't good enough to autocomplete.
    db->GetRowForURL(search_base, &info);
    promote = match.url_info.typed_count() <= 1;
  }

  // Promote or add the desired URL to the list of matches.
  EnsureMatchPresent(info, match.input_location, match.match_in_scheme,
                     matches, promote);
}

// static
void HistoryURLProvider::EnsureMatchPresent(
    const history::URLRow& info,
    std::wstring::size_type input_location,
    bool match_in_scheme,
    HistoryMatches* matches,
    bool promote) {
  // |matches| may already have an entry for this.
  for (HistoryMatches::iterator i(matches->begin()); i != matches->end();
       ++i) {
    if (i->url_info.url() == info.url()) {
      // Rotate it to the front if the caller wishes.
      if (promote)
        std::rotate(matches->begin(), i, i + 1);
      return;
    }
  }

  // No entry, so create one.
  HistoryMatch match(info, input_location, match_in_scheme, true);
  if (promote)
    matches->push_front(match);
  else
    matches->push_back(match);
}

void HistoryURLProvider::RunAutocompletePasses(const AutocompleteInput& input,
                                               bool fixup_input_and_run_pass_1) {
  matches_.clear();

  if ((input.type() != AutocompleteInput::UNKNOWN) &&
      (input.type() != AutocompleteInput::REQUESTED_URL) &&
      (input.type() != AutocompleteInput::URL))
    return;

  // Create a match for exactly what the user typed.  This will always be one
  // of the top two results we return.
  const bool trim_http = !url_util::FindAndCompareScheme(input.text(),
                                                         "http", NULL);
  SuggestExactInput(input, trim_http);

  // We'll need the history service to run both passes, so try to obtain it.
  HistoryService* const history_service = profile_ ?
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS) : history_service_;
  if (!history_service)
    return;

  // Create the data structure for the autocomplete passes.  We'll save this off
  // onto the |params_| member for later deletion below if we need to run pass
  // 2.
  const std::wstring& languages = profile_ ?
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages) : std::wstring();
  scoped_ptr<HistoryURLProviderParams> params(
      new HistoryURLProviderParams(input, trim_http, matches_, languages));

  if (fixup_input_and_run_pass_1) {
    // Do some fixup on the user input before matching against it, so we provide
    // good results for local file paths, input with spaces, etc.
    // NOTE: This purposefully doesn't take input.desired_tld() into account; if
    // it did, then holding "ctrl" would change all the results from the
    // HistoryURLProvider provider, not just the What You Typed Result.
    // However, this means we need to call this _after_ calling
    // SuggestExactInput(), since that function does need to take
    // input.desired_tld() into account; if it doesn't, it may convert "56" +
    // ctrl into "0.0.0.56.com" instead of "56.com" like the user probably
    // wanted.  It's not a problem to call this after SuggestExactInput(),
    // because that function fixes up the user's input in a way that's a
    // superset of what FixupUserInput() does.
    const std::wstring fixed_text(FixupUserInput(input.text()));
    if (fixed_text.empty()) {
      // Conceivably fixup could result in an empty string (although I don't
      // have cases where this happens offhand).  We can't do anything with
      // empty input, so just bail; otherwise we'd crash later.
      return;
    }
    params->input.set_text(fixed_text);

    // Pass 1: Get the in-memory URL database, and use it to find and promote
    // the inline autocomplete match, if any.
    history::URLDatabase* url_db = history_service->in_memory_database();
    // url_db can be NULL if it hasn't finished initializing (or failed to
    // initialize).  In this case all we can do is fall back on the second
    // pass.  Ultimately, we should probably try to ensure the history system
    // starts properly before we get here, as otherwise this can cause
    // inconsistent behavior when the user has just started the browser and
    // tries to type immediately.
    if (url_db) {
      DoAutocomplete(NULL, url_db, params.get());
      // params->matches now has the matches we should expose to the provider.
      // Since pass 2 expects a "clean slate" set of matches that only contains
      // the not-yet-fixed-up What You Typed match, which is exactly what
      // matches_ currently contains, just swap them.
      matches_.swap(params->matches);
      UpdateStarredStateOfMatches();
    }
  }

  // Pass 2: Ask the history service to call us back on the history thread,
  // where we can read the full on-disk DB.
  if (!input.synchronous_only()) {
    done_ = false;
    params_ = params.release();  // This object will be destroyed in
                                 // QueryComplete() once we're done with it.
    history_service->ScheduleAutocomplete(this, params_);
  }
}

const HistoryURLProvider::Prefix* HistoryURLProvider::BestPrefix(
    const GURL& url,
    const std::wstring& prefix_suffix) const {
  const Prefix* best_prefix = NULL;
  const std::wstring text(UTF8ToWide(url.spec()));
  for (Prefixes::const_iterator i(prefixes_.begin()); i != prefixes_.end();
       ++i) {
    if ((best_prefix == NULL) ||
        (i->num_components > best_prefix->num_components)) {
      std::wstring prefix_with_suffix(i->prefix + prefix_suffix);
      if ((text.length() >= prefix_with_suffix.length()) &&
          !text.compare(0, prefix_with_suffix.length(), prefix_with_suffix))
        best_prefix = &(*i);
    }
  }
  return best_prefix;
}

void HistoryURLProvider::SortMatches(HistoryMatches* matches) const {
  // Sort by quality, best first.
  std::sort(matches->begin(), matches->end(), &CompareHistoryMatch);

  // Remove duplicate matches (caused by the search string appearing in one of
  // the prefixes as well as after it).  Consider the following scenario:
  //
  // User has visited "http://http.com" once and "http://htaccess.com" twice.
  // User types "http".  The autocomplete search with prefix "http://" returns
  // the first host, while the search with prefix "" returns both hosts.  Now
  // we sort them into rank order:
  //   http://http.com     (innermost_match)
  //   http://htaccess.com (!innermost_match, url_info.visit_count == 2)
  //   http://http.com     (!innermost_match, url_info.visit_count == 1)
  //
  // The above scenario tells us we can't use std::unique(), since our
  // duplicates are not always sequential.  It also tells us we should remove
  // the lower-quality duplicate(s), since otherwise the returned results won't
  // be ordered correctly.  This is easy to do: we just always remove the later
  // element of a duplicate pair.
  // Be careful!  Because the vector contents may change as we remove elements,
  // we use an index instead of an iterator in the outer loop, and don't
  // precalculate the ending position.
  for (size_t i = 0; i < matches->size(); ++i) {
    HistoryMatches::iterator j(matches->begin() + i + 1);
    while (j != matches->end()) {
      if ((*matches)[i].url_info.url() == j->url_info.url())
        j = matches->erase(j);
      else
        ++j;
    }
  }
}

void HistoryURLProvider::CullPoorMatches(HistoryMatches* matches) const {
  static const int kLowQualityMatchTypedLimit = 1;
  static const int kLowQualityMatchVisitLimit = 3;
  static const int kLowQualityMatchAgeLimitInDays = 3;
  Time recent_threshold =
      Time::Now() - TimeDelta::FromDays(kLowQualityMatchAgeLimitInDays);
  for (HistoryMatches::iterator i(matches->begin()); i != matches->end();) {
    const history::URLRow& url_info = i->url_info;
    if ((url_info.typed_count() <= kLowQualityMatchTypedLimit) &&
        (url_info.visit_count() <= kLowQualityMatchVisitLimit) &&
        (url_info.last_visit() < recent_threshold)) {
      i = matches->erase(i);
    } else {
      ++i;
    }
  }
}

void HistoryURLProvider::CullRedirects(history::HistoryBackend* backend,
                                       HistoryMatches* matches,
                                       size_t max_results) const {
  for (size_t source = 0;
       (source < matches->size()) && (source < max_results); ) {
    const GURL& url = (*matches)[source].url_info.url();
    // TODO(brettw) this should go away when everything uses GURL.
    HistoryService::RedirectList redirects;
    backend->GetMostRecentRedirectsFrom(url, &redirects);
    if (!redirects.empty()) {
      // Remove all but the first occurrence of any of these redirects in the
      // search results. We also must add the URL we queried for, since it may
      // not be the first match and we'd want to remove it.
      //
      // For example, when A redirects to B and our matches are [A, X, B],
      // we'll get B as the redirects from, and we want to remove the second
      // item of that pair, removing B. If A redirects to B and our matches are
      // [B, X, A], we'll want to remove A instead.
      redirects.push_back(url);
      source = RemoveSubsequentMatchesOf(matches, source, redirects);
    } else {
      // Advance to next item.
      source++;
    }
  }

  if (matches->size() > max_results)
    matches->resize(max_results);
}

size_t HistoryURLProvider::RemoveSubsequentMatchesOf(
    HistoryMatches* matches,
    size_t source_index,
    const std::vector<GURL>& remove) const {
  size_t next_index = source_index + 1;  // return value = item after source

  // Find the first occurrence of any URL in the redirect chain. We want to
  // keep this one since it is rated the highest.
  HistoryMatches::iterator first(std::find_first_of(
      matches->begin(), matches->end(), remove.begin(), remove.end()));
  DCHECK(first != matches->end()) <<
      "We should have always found at least the original URL.";

  // Find any following occurrences of any URL in the redirect chain, these
  // should be deleted.
  HistoryMatches::iterator next(first);
  next++;  // Start searching immediately after the one we found already.
  while (next != matches->end() &&
         (next = std::find_first_of(next, matches->end(), remove.begin(),
                                    remove.end())) != matches->end()) {
    // Remove this item. When we remove an item before the source index, we
    // need to shift it to the right and remember that so we can return it.
    next = matches->erase(next);
    if (static_cast<size_t>(next - matches->begin()) < next_index)
      next_index--;
  }
  return next_index;
}

AutocompleteMatch HistoryURLProvider::HistoryMatchToACMatch(
    HistoryURLProviderParams* params,
    const HistoryMatch& history_match,
    MatchType match_type,
    size_t match_number) {
  const history::URLRow& info = history_match.url_info;
  AutocompleteMatch match(this,
      CalculateRelevance(params->input.type(), match_type, match_number),
      !!info.visit_count(), AutocompleteMatch::HISTORY_URL);
  match.destination_url = info.url();
  match.fill_into_edit = gfx::ElideUrl(info.url(), ChromeFont(), 0,
      match_type == WHAT_YOU_TYPED ? std::wstring() : params->languages);
  if (!params->input.prevent_inline_autocomplete()) {
    match.inline_autocomplete_offset =
        history_match.input_location + params->input.text().length();
  }
  size_t offset = 0;
  if (params->trim_http && !history_match.match_in_scheme) {
    offset = TrimHttpPrefix(&match.fill_into_edit);
    if (match.inline_autocomplete_offset != std::wstring::npos) {
      DCHECK(match.inline_autocomplete_offset >= offset);
      match.inline_autocomplete_offset -= offset;
    }
  }
  DCHECK((match.inline_autocomplete_offset == std::wstring::npos) ||
         (match.inline_autocomplete_offset <= match.fill_into_edit.length()));

  match.contents = match.fill_into_edit;
  AutocompleteMatch::ClassifyLocationInString(
      history_match.input_location - offset, params->input.text().length(),
      match.contents.length(), ACMatchClassification::URL,
      &match.contents_class);
  match.description = info.title();
  AutocompleteMatch::ClassifyMatchInString(params->input.text(), info.title(),
                                           ACMatchClassification::NONE,
                                           &match.description_class);

  return match;
}

