// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_H_

#include <map>
#include <string>
#include <vector>
#include "base/logging.h"
#include "base/ref_counted.h"
#include "chrome/common/page_transition_types.h"
#include "googleurl/src/url_parse.h"

// The AutocompleteController is the center of the autocomplete system.  A
// class implementing AutocompleteController::Listener creates an instance of
// the controller, which in turn creates a set of AutocompleteProviders to
// serve it.  The listener can ask the controller to Start() a query; the
// controller in turn passes this call down to the providers, each of which
// keeps track of its own results and whether it has finished processing the
// query.  When a provider gets more results or finishes processing, it
// notifies the controller, which merges the combined results together and
// returns them to the listener.
//
// The listener may also cancel the current query by calling Stop(), which the
// controller will in turn communicate to all the providers.  No callbacks will
// happen after a request has been stopped.
//
// IMPORTANT: There is NO THREAD SAFETY built into this portion of the
// autocomplete system.  All calls to and from the AutocompleteController should
// happen on the same thread.  AutocompleteProviders are responsible for doing
// their own thread management when they need to return results asynchronously.
//
// The AutocompleteProviders each return one kind of results, such as history
// results or search results.  These results are given "relevance" scores.
// Historically the relevance for each column added up to 100, then scores
// were from 1-100. Both have proved a bit painful, and will be changed going
// forward. The important part is that higher relevance scores are more
// important than lower relevance scores. The relevance scores and class
// providing the result are as follows:
//
// UNKNOWN input type:
// --------------------------------------------------------------------|-----
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// HistoryURL (exact or inline autocomplete match)                     | 1400
// Search (what you typed)                                             | 1300
// HistoryURL (what you typed)                                         | 1200
// Keyword (substituting, exact match)                                 | 1100
// Search (past query in history)                                      | 1050--
// HistoryContents (any match in title of starred page)                | 1000++
// HistoryURL (inexact match)                                          |  900++
// Search (navigational suggestion)                                    |  800++
// HistoryContents (any match in title of nonstarred page)             |  700++
// Search (suggestion)                                                 |  600++
// HistoryContents (any match in body of starred page)                 |  550++
// HistoryContents (any match in body of nonstarred page)              |  500++
// Keyword (inexact match)                                             |  450
//
// REQUESTED_URL input type:
// --------------------------------------------------------------------|-----
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// HistoryURL (exact or inline autocomplete match)                     | 1400
// HistoryURL (what you typed)                                         | 1300
// Search (what you typed)                                             | 1200
// Keyword (substituting, exact match)                                 | 1100
// Search (past query in history)                                      | 1050--
// HistoryContents (any match in title of starred page)                | 1000++
// HistoryURL (inexact match)                                          |  900++
// Search (navigational suggestion)                                    |  800++
// HistoryContents (any match in title of nonstarred page)             |  700++
// Search (suggestion)                                                 |  600++
// HistoryContents (any match in body of starred page)                 |  550++
// HistoryContents (any match in body of nonstarred page)              |  500++
// Keyword (inexact match)                                             |  450
//
// URL input type:
// --------------------------------------------------------------------|-----
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// HistoryURL (exact or inline autocomplete match)                     | 1400
// HistoryURL (what you typed)                                         | 1200
// Keyword (substituting, exact match)                                 | 1100
// HistoryURL (inexact match)                                          |  900++
// Search (what you typed)                                             |  850
// Search (navigational suggestion)                                    |  800++
// Search (past query in history)                                      |  750--
// Keyword (inexact match)                                             |  700
// Search (suggestion)                                                 |  300++
//
// QUERY input type:
// --------------------------------------------------------------------|-----
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// Keyword (substituting, exact match)                                 | 1400
// Search (what you typed)                                             | 1300
// Search (past query in history)                                      | 1250--
// HistoryContents (any match in title of starred page)                | 1200++
// Search (navigational suggestion)                                    | 1000++
// HistoryContents (any match in title of nonstarred page)             |  900++
// Search (suggestion)                                                 |  800++
// HistoryContents (any match in body of starred page)                 |  750++
// HistoryContents (any match in body of nonstarred page)              |  700++
// Keyword (inexact match)                                             |  650
//
// FORCED_QUERY input type:
// --------------------------------------------------------------------|-----
// Search (what you typed)                                             | 1500
// Search (past query in history)                                      | 1250--
// HistoryContents (any match in title of starred page)                | 1200++
// Search (navigational suggestion)                                    | 1000++
// HistoryContents (any match in title of nonstarred page)             |  900++
// Search (suggestion)                                                 |  800++
// HistoryContents (any match in body of starred page)                 |  750++
// HistoryContents (any match in body of nonstarred page)              |  700++
//
// (A search keyword is a keyword with a replacement string; a bookmark keyword
// is a keyword with no replacement string, that is, a shortcut for a URL.)
//
// The value column gives the ranking returned from the various providers.
// ++: a series of results with relevance from n up to (n + max_matches).
// --: relevance score falls off over time (discounted 50 points @ 15 minutes,
//     450 points @ two weeks)

class AutocompleteInput;
struct AutocompleteMatch;
class AutocompleteProvider;
class AutocompleteResult;
class AutocompleteController;
class GURL;
class HistoryContentsProvider;
class KeywordProvider;
class Profile;
class TemplateURL;

typedef std::vector<AutocompleteMatch> ACMatches;
typedef std::vector<AutocompleteProvider*> ACProviders;

// AutocompleteInput ----------------------------------------------------------

// The user input for an autocomplete query.  Allows copying.
class AutocompleteInput {
 public:
  enum Type {
    INVALID,       // Empty input
    UNKNOWN,       // Valid input whose type cannot be determined
    REQUESTED_URL, // Input autodetected as UNKNOWN, which the user wants to
                   // treat as an URL by specifying a desired_tld
    URL,           // Input autodetected as a URL
    QUERY,         // Input autodetected as a query
    FORCED_QUERY,  // Input forced to be a query by an initial '?'
  };

  AutocompleteInput()
      : type_(INVALID),
        prevent_inline_autocomplete_(false),
        prefer_keyword_(false) {
  }
  
  AutocompleteInput(const std::wstring& text,
                    const std::wstring& desired_tld,
                    bool prevent_inline_autocomplete,
                    bool prefer_keyword);

  // Parses |text| and returns the type of input this will be interpreted as.
  // The components of the input are stored in the output parameter |parts|.
  static Type Parse(const std::wstring& text,
                    const std::wstring& desired_tld,
                    url_parse::Parsed* parts,
                    std::wstring* scheme);

  // User-provided text to be completed.
  const std::wstring& text() const { return text_; }

  // Use of this setter is risky, since no other internal state is updated
  // besides |text_|.  Only callers who know that they're not changing the
  // type/scheme/etc. should use this.
  void set_text(const std::wstring& text) { text_ = text; }

  // The type of input supplied.
  Type type() const { return type_; }

  // The scheme parsed from the provided text; only meaningful when type_ is
  // URL.
  const std::wstring& scheme() const { return scheme_; }

  // User's desired TLD, if one is not already present in the text to
  // autocomplete.  When this is non-empty, it also implies that "www." should
  // be prepended to the domain where possible.  This should not have a leading
  // '.' (use "com" instead of ".com").
  const std::wstring& desired_tld() const { return desired_tld_; }

  // Returns whether inline autocompletion should be prevented.
  const bool prevent_inline_autocomplete() const {
    return prevent_inline_autocomplete_;
  }

  // Returns whether, given an input string consisting solely of a substituting
  // keyword, we should score it like a non-substituting keyword.
  const bool prefer_keyword() const { return prefer_keyword_; }

  // operator==() by another name.
  bool Equals(const AutocompleteInput& other) const;

  // Resets all internal variables to the null-constructed state.
  void Clear();

  // Returns parsed URL components.
  const url_parse::Parsed& parts() const { return parts_; }

 private:
  std::wstring text_;
  Type type_;
  url_parse::Parsed parts_;
  std::wstring scheme_;
  std::wstring desired_tld_;
  bool prevent_inline_autocomplete_;
  bool prefer_keyword_;
};

// AutocompleteMatch ----------------------------------------------------------

// A single result line with classified spans.  The autocomplete popup displays
// the 'contents' and the 'description' (the description is optional) in the
// autocomplete dropdown, and fills in 'fill_into_edit' into the textbox when
// that line is selected.  fill_into_edit may be the same as 'description' for
// things like URLs, but may be different for searches or other providers.  For
// example, a search result may say "Search for asdf" as the description, but
// "asdf" should appear in the box.
struct AutocompleteMatch {
  // Autocomple results return strings that are classified according to a
  // separate vector of styles. This vector must be sorted, and associates
  // flags with portions of the strings. It is required that all text be
  // inside a classification range. Even if you have no classification, you
  // should create an entry at offset 0 with no flags.
  //
  // Example: The user typed "goog"
  //   http://www.google.com/        Google
  //   ^          ^   ^              ^   ^
  //   0,         |   15,            |   4,
  //              11,match           0,match
  //
  // This structure holds the classifiction information for each span.
  struct ACMatchClassification {
    // The values in here are not mutually exclusive -- use them like a
    // bitfield.  This also means we use "int" instead of this enum type when
    // passing the values around, so the compiler doesn't complain.
    enum Style {
      NONE  = 0,
      URL   = 1 << 0,  // A URL
      MATCH = 1 << 1,  // A match for the user's search term
      DIM   = 1 << 2,  // "Helper text"
    };

    ACMatchClassification(size_t offset, int style)
        : offset(offset),
          style(style) {
    }

    // Offset within the string that this classification starts
    size_t offset;

    int style;
  };

  typedef std::vector<ACMatchClassification> ACMatchClassifications;

  // The type of this match.
  // URL: a url, typically one the user previously entered but it may have
  //      also been suggested. This is the default.
  // KEYWORD: a keyword.
  // SEARCH: short cut for typing type into the Google homepage. This should
  //         only be used if the full URL is not shown.
  enum Type {
    // Something that looks like a URL ("http://foo.com", "internal-server/").
    // This is the default.
    URL,

    // A manually created or auto-generated keyword, with or without a query
    // component. Auto-generated keywords may look similar to urls. See
    // keyword_autocomplete.cc.
    KEYWORD,

    // A search term or phrase for the user's default search provider
    // ("games", "foo"). These visually look similar to keywords. See
    // google_autocomplete.cc.
    SEARCH,

    // Shortcut that takes the user to destinations->history.
    HISTORY_SEARCH
  };

  AutocompleteMatch();
  AutocompleteMatch(AutocompleteProvider* provider,
                    int relevance,
                    bool deletable);

  // Comparison function for determining when one match is better than another.
  static bool MoreRelevant(const AutocompleteMatch& elem1,
                           const AutocompleteMatch& elem2);

  // Comparison functions for removing matches with duplicate destinations.
  static bool DestinationSortFunc(const AutocompleteMatch& elem1,
                                  const AutocompleteMatch& elem2);
  static bool DestinationsEqual(const AutocompleteMatch& elem1,
                                const AutocompleteMatch& elem2);

  // Helper functions for classes creating matches:
  // Fills in the classifications for |text|, using |style| as the base style
  // and marking the first instance of |find_text| as a match.  (This match
  // will also not be dimmed, if |style| has DIM set.)
  static void ClassifyMatchInString(const std::wstring& find_text,
                                    const std::wstring& text,
                                    int style,
                                    ACMatchClassifications* classifications);

  // Similar to ClassifyMatchInString(), but for cases where the range to mark
  // as matching is already known (avoids calling find()).  This can be helpful
  // when find() would be misleading (e.g. you want to mark the second match in
  // a string instead of the first).
  static void ClassifyLocationInString(size_t match_location,
                                       size_t match_length,
                                       size_t overall_length,
                                       int style,
                                       ACMatchClassifications* classifications);

  // The provider of this match, used to  remember which provider the user had
  // selected when the input changes. This may be NULL, in which case there is
  // no provider (or memory of the user's selection).
  AutocompleteProvider* provider;

  // The relevance of this match. See table above for scores returned by
  // various providers. This is used to rank matches among all responding
  // providers, so different providers must be carefully tuned to supply
  // matches with appropriate relevance.
  //
  // If the relevance is negative, it will only be displayed if there are not
  // enough non-negative items in all the providers to max out the popup. In
  // this case, the relevance of the additional items will be inverted so they
  // can be mixed in with the rest of the relevances. This allows a provider
  // to group its results, having the added items appear intermixed with its
  // other results.
  //
  // TODO(pkasting): http://b/1111299 This should be calculated algorithmically,
  // rather than being a fairly fixed value defined by the table above.
  int relevance;

  // True if the user should be able to delete this match.
  bool deletable;

  // This string is loaded into the location bar when the item is selected
  // by pressing the arrow keys. This may be different than a URL, for example,
  // for search suggestions, this would just be the search terms.
  std::wstring fill_into_edit;

  // The position within fill_into_edit from which we'll display the inline
  // autocomplete string.  This will be std::wstring::npos if this match should
  // not be inline autocompleted.
  size_t inline_autocomplete_offset;

  // The URL to actually load when the autocomplete item is selected. This URL
  // should be canonical so we can compare URLs with strcmp to avoid dupes.
  // It may be empty if there is no possible navigation.
  std::wstring destination_url;

  // The text displayed on the left in the search results
  std::wstring contents;
  ACMatchClassifications contents_class;

  // Displayed to the right of the result as the title or other helper info
  std::wstring description;
  ACMatchClassifications description_class;

  // The transition type to use when the user opens this match.  By default
  // this is TYPED.  Providers whose matches do not look like URLs should set
  // it to GENERATED.
  PageTransition::Type transition;

  // True when this match is the "what you typed" match from the history
  // system.
  bool is_history_what_you_typed_match;

  // Type of this match.
  Type type;

  // If this match corresponds to a keyword, this is the TemplateURL the
  // keyword was obtained from.
  const TemplateURL* template_url;

  // True if the user has starred the destination URL.
  bool starred;

#ifndef NDEBUG
  // Does a data integrity check on this match.
  void Validate() const;

  // Checks one text/classifications pair for valid values.
  void ValidateClassifications(
      const std::wstring& text,
      const ACMatchClassifications& classifications) const;
#endif
};

typedef AutocompleteMatch::ACMatchClassification ACMatchClassification;
typedef std::vector<ACMatchClassification> ACMatchClassifications;

// AutocompleteProvider -------------------------------------------------------

// A single result provider for the autocomplete system.  Given user input, the
// provider decides what (if any) matches to return, their relevance, and their
// classifications.
class AutocompleteProvider
    : public base::RefCountedThreadSafe<AutocompleteProvider> {
 public:
  class ACProviderListener {
   public:
    // Called by a provider as a notification that something has changed.
    // |updated_matches| should be true iff the matches have changed in some
    // way (they may not have changed if, for example, the provider did an
    // asynchronous query to get more results, came up with none, and is now
    // giving up).
    //
    // NOTE: Providers MUST only call this method while processing asynchronous
    // queries.  Do not call this for a synchronous query.
    //
    // NOTE: There's no parameter to tell the listener _which_ provider is
    // calling it.  Because the AutocompleteController (the typical listener)
    // doesn't cache the providers' individual results locally, it has to get
    // them all again when this is called anyway, so such a parameter wouldn't
    // actually be useful.
    virtual void OnProviderUpdate(bool updated_matches) = 0;
  };

  AutocompleteProvider(ACProviderListener* listener,
                       Profile* profile,
                       char* name)
      : listener_(listener),
        profile_(profile),
        done_(true),
        name_(name) {
  }

  virtual ~AutocompleteProvider();

  // Invoked when the profile changes.
  void SetProfile(Profile* profile);

  // Called to start an autocomplete query.  The provider is responsible for
  // tracking its results for this query and whether it is done processing the
  // query.  When new results are available or the provider finishes, it
  // calls the controller's OnProviderUpdate() method.  The controller can then
  // get the new results using the provider's accessors.
  // Exception: Results available immediately after starting the query (that
  // is, synchronously) do not cause any notifications to be sent.  The
  // controller is expected to check for these without prompting (since
  // otherwise, starting each provider running would result in a flurry of
  // notifications).
  //
  // Once Stop() has been called, no more notifications should be sent.
  //
  // |minimal_changes| is an optimization that lets the provider do less work
  // when the |input|'s text hasn't changed.  See the body of
  // AutocompletePopupModel::StartAutocomplete().
  //
  // If |synchronous_only| is true, no asynchronous work should be scheduled;
  // the provider should stop after it has returned all the
  // synchronously-available results.  This also means any in-progress
  // asynchronous work should be canceled, so the provider does not call back at
  // a later time.
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes,
                     bool synchronous_only) = 0;

  // Called when a provider must not make any more callbacks for the current
  // query.
  virtual void Stop() {
    done_ = true;
  }

  // Returns the set of matches for the current query.
  const ACMatches& matches() const { return matches_; }

  // Returns whether the provider is done processing the query.
  bool done() const { return done_; }

  // Returns the name of this provider.
  const char* name() const { return name_; }

  // Called to delete a match and the backing data that produced it.  This
  // match should not appear again in this or future queries.  This can only be
  // called for matches the provider marks as deletable.
  // NOTE: Remember to call OnProviderUpdate() if matches_ is updated.
  virtual void DeleteMatch(const AutocompleteMatch& match) {}

  static void set_max_matches(size_t max_matches) {
    max_matches_ = max_matches;
  }

  static size_t max_matches() { return max_matches_; }

 protected:
  // Updates the starred state of each of the matches in matches_ from the
  // profile's bookmark bar model.
  void UpdateStarredStateOfMatches();

  // The profile associated with the AutocompleteProvider.  Reference is not
  // owned by us.
  Profile* profile_;

  ACProviderListener* listener_;
  ACMatches matches_;
  bool done_;

  // The name of this provider.  Used for logging.
  const char* name_;

  // A convenience function to call gfx::ElideUrl() with the current set of
  // "Accept Languages" when check_accept_lang is true. Otherwise, it's called
  // with an empty list.
  std::wstring StringForURLDisplay(const GURL& url, bool check_accept_lang);

 private:
  // A suggested upper bound for how many matches a provider should return.
  // TODO(pkasting): http://b/1111299 , http://b/933133 This should go away once
  // we have good relevance heuristics; the controller should handle all
  // culling.
  static size_t max_matches_;

  DISALLOW_EVIL_CONSTRUCTORS(AutocompleteProvider);
};

typedef AutocompleteProvider::ACProviderListener ACProviderListener;

// AutocompleteResult ---------------------------------------------------------

// All matches from all providers for a particular query.  This also tracks
// what the default match should be if the user doesn't manually select another
// match.
class AutocompleteResult {
 public:
  typedef ACMatches::const_iterator const_iterator;
  typedef ACMatches::iterator iterator;

  // The "Selection" struct is the information we need to select the same match
  // in one result set that was selected in another.
  struct Selection {
    Selection()
        : provider_affinity(NULL),
          is_history_what_you_typed_match(false) {
    }

    // Clear the selection entirely.
    void Clear();

    // True when the selection is empty.
    bool empty() const {
      return destination_url.empty() && !provider_affinity &&
          !is_history_what_you_typed_match;
    }

    // The desired destination URL.
    std::wstring destination_url;

    // The desired provider.  If we can't find a match with the specified
    // |destination_url|, we'll use the best match from this provider.
    const AutocompleteProvider* provider_affinity;

    // True when this is the HistoryURLProvider's "what you typed" match.  This
    // can't be tracked using |destination_url| because its URL changes on every
    // keystroke, so if this is set, we'll preserve the selection by simply
    // choosing the new "what you typed" entry and ignoring |destination_url|.
    bool is_history_what_you_typed_match;
  };

  static void set_max_matches(size_t max_matches) {
    max_matches_ = max_matches;
  }
  static size_t max_matches() { return max_matches_; }

  AutocompleteResult();

  // operator=() by another name.
  void CopyFrom(const AutocompleteResult& rhs);

  // Adds a single match. The match is inserted at the appropriate position
  // based on relevancy and display order. This is ONLY for use after
  // SortAndCull() has been invoked, and preserves default_match_.
  void AddMatch(const AutocompleteMatch& match);

  // Adds a new set of matches to the set of results.  Does not re-sort.
  void AppendMatches(const ACMatches& matches);

  // Removes duplicates, puts the list in sorted order and culls to leave only
  // the best kMaxMatches results.  Sets the default match to the best match.
  void SortAndCull();

  // Vector-style accessors/operators.
  size_t size() const { return matches_.size(); }
  bool empty() const { return matches_.empty(); }
  const_iterator begin() const { return matches_.begin(); }
  iterator begin() { return matches_.begin(); }
  const_iterator end() const { return matches_.end(); }
  iterator end() { return matches_.end(); }

  // Returns the match at the given index.
  const AutocompleteMatch& match_at(size_t index) const {
    DCHECK(index < matches_.size());
    return matches_[index];
  }

  // Get the default match for the query (not necessarily the first).  Returns
  // end() if there is no default match.
  const_iterator default_match() const { return default_match_; }

  // Given some input and a particular match in this result set, returns the
  // "alternate navigation URL", if any, for that match.  This is a URL to try
  // offering as a navigational option in case the user didn't actually mean to
  // navigate to the URL of |match|.  For example, if the user's local intranet
  // contains site "foo", and the user types "foo", we default to searching for
  // "foo" when the user may have meant to navigate there.  In cases like this,
  // |match| will point to the "search for 'foo'" result, and this function will
  // return "http://foo/".
  std::wstring GetAlternateNavURL(const AutocompleteInput& input,
                                  const_iterator match) const;

  // Releases the resources associated with this object. Some callers may
  // want to perform several searches without creating new results each time.
  // They can call this function to re-use the result for another query.
  void Reset() {
    matches_.clear();
    default_match_ = end();
  }

#ifndef NDEBUG
  // Does a data integrity check on this result.
  void Validate() const;
#endif

 private:
  // Max number of matches we'll show from the various providers. We may end
  // up showing an additional shortcut for Destinations->History, see
  // AddHistoryContentsShortcut.
  static size_t max_matches_;
  ACMatches matches_;
  const_iterator default_match_;

  DISALLOW_EVIL_CONSTRUCTORS(AutocompleteResult);
};

// AutocompleteController -----------------------------------------------------

// The coordinator for autocomplete queries, responsible for combining the
// results from a series of providers into one AutocompleteResult and
// interacting with the Listener that owns it.
class AutocompleteController : public ACProviderListener {
 public:
  class ACControllerListener {
   public:
    // Called by the controller when new results are available and/or the query
    // is complete.  The listener can then call GetResult() and provide an
    // AutocompleteResult* to be filled in.
    //
    // Note that this function is never called for synchronous_only queries
    // (see Start()).  If you're only using those, you can create the controller
    // with a NULL listener.
    virtual void OnAutocompleteUpdate(bool updated_result,
                                      bool query_complete) = 0;
  };

  // Used to indicate an index that is not selected in a call to Update()
  // and for merging results.
  static const int kNoItemSelected;

  // Normally, you will call the first constructor.  Unit tests can use the
  // second to set the providers to some known testing providers.  The default
  // providers will be overridden and the controller will take ownership of the
  // providers, Release()ing them on destruction.
  //
  // It is safe to pass NULL for |listener| iff you only ever use synchronous
  // queries.
  AutocompleteController(ACControllerListener* listener, Profile* profile);
#ifdef UNIT_TEST
  AutocompleteController(ACControllerListener* listener,
                         const ACProviders& providers)
      : listener_(listener),
        providers_(providers),
        history_contents_provider_(NULL) {
  }
#endif
  ~AutocompleteController();

  // Invoked when the profile changes. This forwards the call down to all
  // the AutocompleteProviders.
  void SetProfile(Profile* profile);

  // Starts an autocomplete query, which continues until all providers are
  // done or the query is Stop()ed.  It is safe to Start() a new query without
  // Stop()ing the previous one.
  //
  // If |minimal_changes| is true, |input| is the same as in the previous
  // query, except for a different desired_tld_ and possibly type_.  Most
  // providers should just be able to recalculate priorities in this case and
  // return synchronously, or at least faster than otherwise.
  //
  // If |synchronous_only| is true, the controller asks the providers to only
  // return results which are synchronously available, which should mean that
  // all providers will be done immediately.
  //
  // The controller does not notify the listener about any results available
  // immediately; the caller should call GetResult() manually if it wants
  // these.  The return value is whether the query is complete; if it is
  // false, then the controller will call OnAutocompleteUpdate() with future
  // result updates (unless the query is Stop()ed).
  bool Start(const AutocompleteInput& input,
             bool minimal_changes,
             bool synchronous_only);

  // Cancels the current query, ensuring there will be no future callbacks to
  // OnAutocompleteUpdate() (until Start() is called again).
  void Stop() const;

  // Called by the listener to get the current results of the query.
  void GetResult(AutocompleteResult* result);

  // From AutocompleteProvider::Listener
  virtual void OnProviderUpdate(bool updated_matches);

 private:
  // Returns true if all providers have finished processing the query.
  bool QueryComplete() const;

  // Returns the number of matches from provider whose destination urls are
  // not in result. first_match is set to the first match whose destination url
  // is NOT in result.
  size_t CountMatchesNotInResult(const AutocompleteProvider* provider,
                                 const AutocompleteResult* result,
                                 AutocompleteMatch* first_match);

  // If the HistoryContentsAutocomplete provider is done and there are more
  // matches in the database than currently shown, an entry is added to
  // result to show all history matches.
  void AddHistoryContentsShortcut(AutocompleteResult* result);

  ACControllerListener* listener_;  // May be NULL.

  // A list of all providers.
  ACProviders providers_;

  HistoryContentsProvider* history_contents_provider_;

  // Input passed to Start.
  AutocompleteInput input_;

  DISALLOW_EVIL_CONSTRUCTORS(AutocompleteController);
};

typedef AutocompleteController::ACControllerListener ACControllerListener;

// AutocompleteLog ------------------------------------------------------------

// The data to log (via the metrics service) when the user selects an item
// from the omnibox popup.
struct AutocompleteLog {
  AutocompleteLog(std::wstring text,
                  size_t selected_index,
                  size_t inline_autocompleted_length,
                  const AutocompleteResult& result)
      : text(text),
        selected_index(selected_index),
        inline_autocompleted_length(inline_autocompleted_length),
        result(result) {
  }
  // The user's input text in the omnibox.
  std::wstring text;
  // Selected index (if selected) or -1 (AutocompletePopupModel::kNoMatch).
  size_t selected_index;
  // Inline autocompleted length (if displayed).
  size_t inline_autocompleted_length;
  // Result set.
  const AutocompleteResult& result;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_H_
