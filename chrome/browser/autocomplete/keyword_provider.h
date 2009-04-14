// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the keyword autocomplete provider. The keyword provider
// is responsible for remembering/suggesting user "search keyword queries"
// (e.g.  "imdb Godzilla") and then fixing them up into valid URLs.  An
// instance of it gets created and managed by the autocomplete controller.
// KeywordProvider uses a TemplateURLModel to find the set of keywords.
//
// For more information on the autocomplete system in general, including how
// the autocomplete controller and autocomplete providers work, see
// chrome/browser/autocomplete.h.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_KEYWORD_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_KEYWORD_PROVIDER_H_

#include <string>

#include "chrome/browser/autocomplete/autocomplete.h"

class Profile;
class TemplateURL;
class TemplateURLModel;

// Autocomplete provider for keyword input.
//
// After construction, the autocomplete controller repeatedly calls Start()
// with some user input, each time expecting to receive a small set of the best
// matches (either synchronously or asynchronously).
//
// To construct these matches, the provider treats user input as a series of
// whitespace-delimited tokens and tries to match the first token as the prefix
// of a known "keyword".  A keyword is some string that maps to a search query
// URL; the rest of the user's input is taken as the input to the query.  For
// example, the keyword "bug" might map to the URL "http://b/issue?id=%s", so
// input like "bug 123" would become "http://b/issue?id=123".
//
// Because we do prefix matching, user input could match more than one keyword
// at once.  (Example: the input "f jazz" matches all keywords starting with
// "f".)  We return the best matches, up to three.
//
// The resulting matches are shown with content specified by the keyword
// (usually "Search [name] for %s"), description "(Keyword: [keyword])", and
// action "[keyword] %s".  If the user has typed a (possibly partial) keyword
// but no search terms, the suggested result is shown greyed out, with
// "<enter term(s)>" as the substituted input, and does nothing when selected.
class KeywordProvider : public AutocompleteProvider {
 public:
  KeywordProvider(ACProviderListener* listener, Profile* profile);
  // For testing.
  KeywordProvider(ACProviderListener* listener, TemplateURLModel* model);

  // Returns the replacement string from the user input. The replacement
  // string is the portion of the input that does not contain the keyword.
  // For example, the replacement string for "b blah" is blah.
  static std::wstring SplitReplacementStringFromInput(
      const std::wstring& input);

  // Returns the matching substituting keyword for |input|, or NULL if there
  // is no keyword for the specified input.
  static const TemplateURL* GetSubstitutingTemplateURLForInput(
      Profile* profile,
      const AutocompleteInput& input,
      std::wstring* remaining_input);

  // AutocompleteProvider
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes);

 private:
  // Extracts the keyword from |input| into |keyword|. Any remaining characters
  // after the keyword are placed in |remaining_input|. Returns true if |input|
  // is valid and has a keyword. This makes use of SplitKeywordFromInput to
  // extract the keyword and remaining string, and uses
  // TemplateURLModel::CleanUserInputKeyword to remove unnecessary characters.
  // In general use this instead of SplitKeywordFromInput.
  static bool ExtractKeywordFromInput(const AutocompleteInput& input,
                                      std::wstring* keyword,
                                      std::wstring* remaining_input);

  // Extracts the next whitespace-delimited token from input and returns it.
  // Sets |remaining_input| to everything after the first token (skipping over
  // intervening whitespace).
  static std::wstring SplitKeywordFromInput(const std::wstring& input,
                                            std::wstring* remaining_input);

  // Fills in the "destination_url" and "contents" fields of |match| with the
  // provided user input and keyword data.
  static void FillInURLAndContents(
      const std::wstring& remaining_input,
      const TemplateURL* element,
      AutocompleteMatch* match);

  // Determines the relevance for some input, given its type, whether the user
  // typed the complete keyword, and whether the keyword needs query text (true
  // if the keyword supports replacement and the user isn't in "prefer keyword
  // matches" mode).
  static int CalculateRelevance(AutocompleteInput::Type type,
                                bool complete,
                                bool no_query_text_needed);

  // Creates a fully marked-up AutocompleteMatch from the user's input.
  AutocompleteMatch CreateAutocompleteMatch(
      TemplateURLModel* model,
      const std::wstring keyword,
      const AutocompleteInput& input,
      size_t prefix_length,
      const std::wstring& remaining_input);

  // Model for the keywords.  This is only non-null when testing, otherwise the
  // TemplateURLModel from the Profile is used.
  TemplateURLModel* model_;

  DISALLOW_EVIL_CONSTRUCTORS(KeywordProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_KEYWORD_PROVIDER_H_
