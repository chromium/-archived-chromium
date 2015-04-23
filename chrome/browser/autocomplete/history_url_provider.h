// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_URL_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_URL_PROVIDER_H_

#include <vector>
#include <deque>

#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/history/history_types.h"

class HistoryService;
class MessageLoop;
class Profile;

namespace history {
class HistoryBackend;
}  // namespace history


// How history autocomplete works
// ==============================
//
// Read down this diagram for temporal ordering.
//
//   Main thread                History thread
//   -----------                --------------
//   AutocompleteController::Start
//     -> HistoryURLProvider::Start
//       -> RunAutocompletePasses
//         -> SuggestExactInput
//         [params_ allocated]
//         -> DoAutocomplete (for inline autocomplete)
//           -> URLDatabase::AutocompleteForPrefix (on in-memory DB)
//         -> HistoryService::ScheduleAutocomplete
//         (return to controller) ----
//                                   /
//                              HistoryBackend::ScheduleAutocomplete
//                                -> HistoryURLProvider::ExecuteWithDB
//                                  -> DoAutocomplete
//                                    -> URLDatabase::AutocompleteForPrefix
//                                /
//   HistoryService::QueryComplete
//     [params_ destroyed]
//     -> AutocompleteProvider::Listener::OnProviderUpdate
//
// The autocomplete controller calls us, and must be called back, on the main
// thread.  When called, we run two autocomplete passes.  The first pass runs
// synchronously on the main thread and queries the in-memory URL database.
// This pass promotes matches for inline autocomplete if applicable.  We do
// this synchronously so that users get consistent behavior when they type
// quickly and hit enter, no matter how loaded the main history database is.
// Doing this synchronously also prevents inline autocomplete from being
// "flickery" in the AutocompleteEdit.  Because the in-memory DB does not have
// redirect data, results other than the top match might change between the
// two passes, so we can't just decide to use this pass' matches as the final
// results.
//
// The second autocomplete pass uses the full history database, which must be
// queried on the history thread.  Start() asks the history service schedule to
// callback on the history thread with a pointer to the main database.  When we
// are done doing queries, we schedule a task on the main thread that notifies
// the AutocompleteController that we're done.
//
// The communication between these threads is done using a
// HistoryURLProviderParams object.  This is allocated in the main thread, and
// normally deleted in QueryComplete().  So that both autocomplete passes can
// use the same code, we also use this to hold results during the first
// autocomplete pass.
//
// While the second pass is running, the AutocompleteController may cancel the
// request.  This can happen frequently when the user is typing quickly.  In
// this case, the main thread sets params_->cancel, which the background thread
// checks periodically.  If it finds the flag set, it stops what it's doing
// immediately and calls back to the main thread.  (We don't delete the params
// on the history thread, because we should only do that when we can safely
// NULL out params_, and that must be done on the main thread.)

// Used to communicate autocomplete parameters between threads via the history
// service.
struct HistoryURLProviderParams {
  HistoryURLProviderParams(const AutocompleteInput& input,
                           bool trim_http,
                           const std::wstring& languages);

  MessageLoop* message_loop;

  // A copy of the autocomplete input. We need the copy since this object will
  // live beyond the original query while it runs on the history thread.
  AutocompleteInput input;

  // Set when "http://" should be trimmed from the beginning of the URLs.
  bool trim_http;

  // Set by the main thread to cancel this request. READ ONLY when running in
  // ExecuteWithDB() on the history thread to prevent deadlock. If this flag is
  // set when the query runs, the query will be abandoned. This allows us to
  // avoid running queries that are no longer needed. Since we don't care if
  // we run the extra queries, the lack of signaling is not a problem.
  bool cancel;

  // List of matches written by the history thread.  We keep this separate list
  // to avoid having the main thread read the provider's matches while the
  // history thread is manipulating them.  The provider copies this list back
  // to matches_ on the main thread in QueryComplete().
  ACMatches matches;

  // Languages we should pass to gfx::GetCleanStringFromUrl.
  std::wstring languages;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(HistoryURLProviderParams);
};

// This class is an autocomplete provider and is also a pseudo-internal
// component of the history system.  See comments above.
//
// Note: This object can get leaked on shutdown if there are pending
// requests on the database (which hold a reference to us). Normally, these
// messages get flushed for each thread. We do a round trip from main, to
// history, back to main while holding a reference. If the main thread
// completes before the history thread, the message to delegate back to the
// main thread will not run and the reference will leak. Therefore, don't do
// anything on destruction.
class HistoryURLProvider : public AutocompleteProvider {
 public:
  HistoryURLProvider(ACProviderListener* listener, Profile* profile)
      : AutocompleteProvider(listener, profile, "HistoryURL"),
        history_service_(NULL),
        prefixes_(GetPrefixes()),
        params_(NULL) {
  }

#ifdef UNIT_TEST
  HistoryURLProvider(ACProviderListener* listener,
                     HistoryService* history_service)
      : AutocompleteProvider(listener, NULL, "History"),
        history_service_(history_service),
        prefixes_(GetPrefixes()),
        params_(NULL) {
  }
#endif
  // no destructor (see note above)

  // AutocompleteProvider
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes);
  virtual void Stop();
  virtual void DeleteMatch(const AutocompleteMatch& match);

  // Runs the history query on the history thread, called by the history
  // system. The history database MAY BE NULL in which case it is not
  // available and we should return no data. Also schedules returning the
  // results to the main thread
  void ExecuteWithDB(history::HistoryBackend* backend,
                     history::URLDatabase* db,
                     HistoryURLProviderParams* params);

  // Actually runs the autocomplete job on the given database, which is
  // guaranteed not to be NULL.
  void DoAutocomplete(history::HistoryBackend* backend,
                      history::URLDatabase* db,
                      HistoryURLProviderParams* params);

  // Dispatches the results to the autocomplete controller. Called on the
  // main thread by ExecuteWithDB when the results are available.
  // Frees params_gets_deleted on exit.
  void QueryComplete(HistoryURLProviderParams* params_gets_deleted);

 private:
  struct Prefix {
    Prefix(std::wstring prefix, int num_components)
        : prefix(prefix),
          num_components(num_components) { }

    std::wstring prefix;

    // The number of "components" in the prefix.  The scheme is a component,
    // and the initial "www." or "ftp." is a component.  So "http://foo.com"
    // and "www.bar.com" each have one component, "ftp://ftp.ftp.com" has two,
    // and "mysite.com" has none.  This is used to tell whether the user's
    // input is an innermost match or not.  See comments in HistoryMatch.
    int num_components;
  };
  typedef std::vector<Prefix> Prefixes;

  // Used for intermediate history result operations.
  struct HistoryMatch {
    // Required for STL, we don't use this directly.
    HistoryMatch()
        : url_info(),
          input_location(std::wstring::npos),
          match_in_scheme(false),
          innermost_match(true) {
    }

    HistoryMatch(const history::URLRow& url_info,
                 size_t input_location,
                 bool match_in_scheme,
                 bool innermost_match)
        : url_info(url_info),
          input_location(input_location),
          match_in_scheme(match_in_scheme),
          innermost_match(innermost_match) {
    }

    bool operator==(const GURL& url) const {
      return url_info.url() == url;
    }

    history::URLRow url_info;

    // The offset of the user's input within the URL.
    size_t input_location;

    // Whether this is a match in the scheme.  This determines whether we'll go
    // ahead and show a scheme on the URL even if the user didn't type one.
    // If our best match was in the scheme, not showing the scheme is both
    // confusing and, for inline autocomplete of the fill_into_edit, dangerous.
    // (If the user types "h" and we match "http://foo/", we need to inline
    // autocomplete that, not "foo/", which won't show anything at all, and
    // will mislead the user into thinking the What You Typed match is what's
    // selected.)
    bool match_in_scheme;

    // A match after any scheme/"www.", if the user input could match at both
    // locations.  If the user types "w", an innermost match ("website.com") is
    // better than a non-innermost match ("www.google.com").  If the user types
    // "x", no scheme in our prefix list (or "www.") begins with x, so all
    // matches are, vacuously,  "innermost matches".
    bool innermost_match;
  };
  typedef std::deque<HistoryMatch> HistoryMatches;

  enum MatchType {
    NORMAL,
    WHAT_YOU_TYPED,
    INLINE_AUTOCOMPLETE
  };

  // Fixes up user URL input to make it more possible to match against.  Among
  // many other things, this takes care of the following:
  // * Prepending file:// to file URLs
  // * Converting drive letters in file URLs to uppercase
  // * Converting case-insensitive parts of URLs (like the scheme and domain)
  //   to lowercase
  // * Convert spaces to %20s
  // Note that we don't do this in AutocompleteInput's constructor, because if
  // e.g. we convert a Unicode hostname to punycode, other providers will show
  // output that surprises the user ("Search Google for xn--6ca.com").
  static std::wstring FixupUserInput(const AutocompleteInput& input);

  // Returns true if |url| is just a host (e.g. "http://www.google.com/") and
  // not some other subpage (e.g. "http://www.google.com/foo.html").
  static bool IsHostOnly(const GURL& url);

  // Acts like the > operator for URLInfo classes.
  static bool CompareHistoryMatch(const HistoryMatch& a,
                                  const HistoryMatch& b);

  // Returns the set of prefixes to use for prefixes_.
  static Prefixes GetPrefixes();

  // Determines the relevance for some input, given its type and which match it
  // is.  If |match_type| is NORMAL, |match_number| is a number
  // [0, kMaxSuggestions) indicating the relevance of the match (higher == more
  // relevant).  For other values of |match_type|, |match_number| is ignored.
  static int CalculateRelevance(AutocompleteInput::Type input_type,
                                MatchType match_type,
                                size_t match_number);

  // Given the user's |input| and a |match| created from it, reduce the
  // match's URL to just a host.  If this host still matches the user input,
  // return it.  Returns the empty string on failure.
  static GURL ConvertToHostOnly(const HistoryMatch& match,
                                const std::wstring& input);

  // See if a shorter version of the best match should be created, and if so
  // place it at the front of |matches|.  This can suggest history URLs that
  // are prefixes of the best match (if they've been visited enough, compared
  // to the best match), or create host-only suggestions even when they haven't
  // been visited before: if the user visited http://example.com/asdf once,
  // we'll suggest http://example.com/ even if they've never been to it.  See
  // the function body for the exact heuristics used.
  static void PromoteOrCreateShorterSuggestion(
      history::URLDatabase* db,
      const HistoryURLProviderParams& params,
      bool have_what_you_typed_match,
      const AutocompleteMatch& what_you_typed_match,
      HistoryMatches* matches);

  // Ensures that |matches| contains an entry for |info|, which may mean adding
  // a new such entry (using |input_location| and |match_in_scheme|).
  //
  // If |promote| is true, this also ensures the entry is the first element in
  // |matches|, moving or adding it to the front as appropriate.  When
  // |promote| is false, existing matches are left in place, and newly added
  // matches are placed at the back.
  static void EnsureMatchPresent(const history::URLRow& info,
                                 std::wstring::size_type input_location,
                                 bool match_in_scheme,
                                 HistoryMatches* matches,
                                 bool promote);

  // Helper function that actually launches the two autocomplete passes.
  void RunAutocompletePasses(const AutocompleteInput& input,
                             bool fixup_input_and_run_pass_1);

  // Returns the best prefix that begins |text|.  "Best" means "greatest number
  // of components".  This may return NULL if no prefix begins |text|.
  //
  // |prefix_suffix| (which may be empty) is appended to every attempted
  // prefix.  This is useful when you need to figure out the innermost match
  // for some user input in a URL.
  const Prefix* BestPrefix(const GURL& text,
                           const std::wstring& prefix_suffix) const;

  // Returns a match corresponding to exactly what the user has typed.
  AutocompleteMatch SuggestExactInput(const AutocompleteInput& input,
                                      bool trim_http);

  // Given a |match| containing the "what you typed" suggestion created by
  // SuggestExactInput(), looks up its info in the DB.  If found, fills in the
  // title from the DB, promotes the match's priority to that of an inline
  // autocomplete match (maybe it should be slightly better?), and places it on
  // the front of |matches| (so we pick the right matches to throw away
  // when culling redirects to/from it).  Returns whether a match was promoted.
  bool FixupExactSuggestion(history::URLDatabase* db,
                            const AutocompleteInput& input,
                            AutocompleteMatch* match,
                            HistoryMatches* matches) const;

  // Determines if |match| is suitable for inline autocomplete, and promotes it
  // if so.
  bool PromoteMatchForInlineAutocomplete(HistoryURLProviderParams* params,
                                         const HistoryMatch& match);

  // Sorts the given list of matches.
  void SortMatches(HistoryMatches* matches) const;

  // Removes results that have been rarely typed or visited, and not any time
  // recently.  The exact parameters for this heuristic can be found in the
  // function body.
  void CullPoorMatches(HistoryMatches* matches) const;

  // Removes results that redirect to each other, leaving at most |max_results|
  // results.
  void CullRedirects(history::HistoryBackend* backend,
                     HistoryMatches* matches,
                     size_t max_results) const;

  // Helper function for CullRedirects, this removes all but the first
  // occurance of [any of the set of strings in |remove|] from the |matches|
  // list.
  //
  // The return value is the index of the item that is after the item in the
  // input identified by |source_index|. If |source_index| or an item before
  // is removed, the next item will be shifted, and this allows the caller to
  // pick up on the next one when this happens.
  size_t RemoveSubsequentMatchesOf(
      HistoryMatches* matches,
      size_t source_index,
      const std::vector<GURL>& remove) const;

  // Converts a line from the database into an autocomplete match for display.
  AutocompleteMatch HistoryMatchToACMatch(HistoryURLProviderParams* params,
                                          const HistoryMatch& history_match,
                                          MatchType match_type,
                                          size_t match_number);

  // This is only non-null for testing, otherwise the HistoryService from the
  // Profile is used.
  HistoryService* history_service_;

  // Prefixes to try appending to user input when looking for a match.
  const Prefixes prefixes_;

  // Params for the current query.  The provider should not free this directly;
  // instead, it is passed as a parameter through the history backend, and the
  // parameter itself is freed once it's no longer needed.  The only reason we
  // keep this member is so we can set the cancel bit on it.
  HistoryURLProviderParams* params_;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_URL_PROVIDER_H_
