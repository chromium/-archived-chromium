// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/keyword_provider.h"

#include <algorithm>
#include <vector>

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"

// static
std::wstring KeywordProvider::SplitReplacementStringFromInput(
    const std::wstring& input) {
  // The input may contain leading whitespace, strip it.
  std::wstring trimmed_input;
  TrimWhitespace(input, TRIM_LEADING, &trimmed_input);

  // And extract the replacement string.
  std::wstring remaining_input;
  SplitKeywordFromInput(trimmed_input, &remaining_input);
  return remaining_input;
}

KeywordProvider::KeywordProvider(ACProviderListener* listener, Profile* profile)
    : AutocompleteProvider(listener, profile, "Keyword"),
      model_(NULL) {
}

KeywordProvider::KeywordProvider(ACProviderListener* listener,
                                 TemplateURLModel* model)
    : AutocompleteProvider(listener, NULL, "Keyword"),
      model_(model) {
}


namespace {

// Helper functor for Start(), for sorting keyword matches by quality.
class CompareQuality {
 public:
  // A keyword is of higher quality when a greater fraction of it has been
  // typed, that is, when it is shorter.
  //
  // TODO(pkasting): http://b/740691 Most recent and most frequent keywords are
  // probably better rankings than the fraction of the keyword typed.  We should
  // always put any exact matches first no matter what, since the code in
  // Start() assumes this (and it makes sense).
  bool operator()(const std::wstring& keyword1,
                  const std::wstring& keyword2) const {
    return keyword1.length() < keyword2.length();
  }
};

}  // namespace

// static
const TemplateURL* KeywordProvider::GetSubstitutingTemplateURLForInput(
    Profile* profile,
    const AutocompleteInput& input,
    std::wstring* remaining_input) {
  std::wstring keyword;
  if (!ExtractKeywordFromInput(input, &keyword, remaining_input))
    return NULL;

  // Make sure the model is loaded. This is cheap and quickly bails out if
  // the model is already loaded.
  TemplateURLModel* model = profile->GetTemplateURLModel();
  DCHECK(model);
  model->Load();

  const TemplateURL* template_url = model->GetTemplateURLForKeyword(keyword);
  return TemplateURL::SupportsReplacement(template_url) ? template_url : NULL;
}

void KeywordProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  matches_.clear();

  // Split user input into a keyword and some query input.
  //
  // We want to suggest keywords even when users have started typing URLs, on
  // the assumption that they might not realize they no longer need to go to a
  // site to be able to search it.  So we call CleanUserInputKeyword() to strip
  // any initial scheme and/or "www.".  NOTE: Any heuristics or UI used to
  // automatically/manually create keywords will need to be in sync with
  // whatever we do here!
  //
  // TODO(pkasting): http://b/1112681 If someday we remember usage frequency for
  // keywords, we might suggest keywords that haven't even been partially typed,
  // if the user uses them enough and isn't obviously typing something else.  In
  // this case we'd consider all input here to be query input.
  std::wstring keyword, remaining_input;
  if (!ExtractKeywordFromInput(input, &keyword, &remaining_input))
    return;

  // Make sure the model is loaded. This is cheap and quickly bails out if
  // the model is already loaded.
  TemplateURLModel* model = profile_ ? profile_->GetTemplateURLModel() : model_;
  DCHECK(model);
  model->Load();

  // Get the best matches for this keyword.
  //
  // NOTE: We could cache the previous keywords and reuse them here in the
  // |minimal_changes| case, but since we'd still have to recalculate their
  // relevances and we can just recreate the results synchronously anyway, we
  // don't bother.
  //
  // TODO(pkasting): http://b/893701 We should remember the user's use of a
  // search query both from the autocomplete popup and from web pages
  // themselves.
  std::vector<std::wstring> keyword_matches;
  model->FindMatchingKeywords(keyword, !remaining_input.empty(),
                              &keyword_matches);
  if (keyword_matches.empty())
    return;
  std::sort(keyword_matches.begin(), keyword_matches.end(), CompareQuality());

  // Limit to one exact or three inexact matches, and mark them up for display
  // in the autocomplete popup.
  // Any exact match is going to be the highest quality match, and thus at the
  // front of our vector.
  if (keyword_matches.front() == keyword) {
    matches_.push_back(CreateAutocompleteMatch(model, keyword, input,
                                               keyword.length(),
                                               remaining_input));
  } else {
    if (keyword_matches.size() > max_matches()) {
      keyword_matches.erase(keyword_matches.begin() + max_matches(),
                            keyword_matches.end());
    }
    for (std::vector<std::wstring>::const_iterator i(keyword_matches.begin());
         i != keyword_matches.end(); ++i) {
      matches_.push_back(CreateAutocompleteMatch(model, *i, input,
                                                 keyword.length(),
                                                 remaining_input));
    }
  }
}

// static
bool KeywordProvider::ExtractKeywordFromInput(const AutocompleteInput& input,
                                              std::wstring* keyword,
                                              std::wstring* remaining_input) {
  if ((input.type() == AutocompleteInput::INVALID) ||
      (input.type() == AutocompleteInput::FORCED_QUERY))
    return false;

  *keyword = TemplateURLModel::CleanUserInputKeyword(
      SplitKeywordFromInput(input.text(), remaining_input));
  return !keyword->empty();
}

// static
std::wstring KeywordProvider::SplitKeywordFromInput(
    const std::wstring& input,
    std::wstring* remaining_input) {
  // Find end of first token.  The AutocompleteController has trimmed leading
  // whitespace, so we need not skip over that.
  const size_t first_white(input.find_first_of(kWhitespaceWide));
  DCHECK(first_white != 0);
  if (first_white == std::wstring::npos)
    return input;  // Only one token provided.

  // Set |remaining_input| to everything after the first token.
  DCHECK(remaining_input != NULL);
  const size_t first_nonwhite(input.find_first_not_of(kWhitespaceWide,
                                                      first_white));
  if (first_nonwhite != std::wstring::npos)
    remaining_input->assign(input.begin() + first_nonwhite, input.end());

  // Return first token as keyword.
  return input.substr(0, first_white);
}

// static
void KeywordProvider::FillInURLAndContents(
    const std::wstring& remaining_input,
    const TemplateURL* element,
    AutocompleteMatch* match) {
  DCHECK(!element->short_name().empty());
  DCHECK(element->url());
  DCHECK(element->url()->IsValid());
  if (remaining_input.empty()) {
    if (element->url()->SupportsReplacement()) {
      // No query input; return a generic, no-destination placeholder.
      match->contents.assign(l10n_util::GetStringF(IDS_KEYWORD_SEARCH,
          element->AdjustedShortNameForLocaleDirection(),
          l10n_util::GetString(IDS_EMPTY_KEYWORD_VALUE)));
      match->contents_class.push_back(
          ACMatchClassification(0, ACMatchClassification::DIM));
    } else {
      // Keyword that has no replacement text (aka a shorthand for a URL).
      match->destination_url = GURL(WideToUTF8(element->url()->url()));
      match->contents.assign(element->short_name());
      AutocompleteMatch::ClassifyLocationInString(0, match->contents.length(),
          match->contents.length(), ACMatchClassification::NONE,
          &match->contents_class);
    }
  } else {
    // Create destination URL by escaping user input and substituting into
    // keyword template URL.  The escaping here handles whitespace in user
    // input, but we rely on later canonicalization functions to do more
    // fixup to make the URL valid if necessary.
    DCHECK(element->url()->SupportsReplacement());
    match->destination_url = GURL(WideToUTF8(element->url()->ReplaceSearchTerms(
      *element, remaining_input, TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
      std::wstring())));
    std::vector<size_t> content_param_offsets;
    match->contents.assign(l10n_util::GetStringF(IDS_KEYWORD_SEARCH,
                                                 element->short_name(),
                                                 remaining_input,
                                                 &content_param_offsets));
    if (content_param_offsets.size() == 2) {
      AutocompleteMatch::ClassifyLocationInString(content_param_offsets[1],
          remaining_input.length(), match->contents.length(),
          ACMatchClassification::NONE, &match->contents_class);
    } else {
      // See comments on an identical NOTREACHED() in search_provider.cc.
      NOTREACHED();
    }
  }
}

// static
int KeywordProvider::CalculateRelevance(AutocompleteInput::Type type,
                                        bool complete,
                                        bool no_query_text_needed) {
  if (complete && no_query_text_needed)
    return 1500;

  switch (type) {
    case AutocompleteInput::UNKNOWN:
    case AutocompleteInput::REQUESTED_URL:
      return complete ? 1100 : 450;

    case AutocompleteInput::URL:
      return complete ? 1100 : 700;

    case AutocompleteInput::QUERY:
      return complete ? 1400 : 650;

    default:
      NOTREACHED();
      return 0;
  }
}

AutocompleteMatch KeywordProvider::CreateAutocompleteMatch(
    TemplateURLModel* model,
    const std::wstring keyword,
    const AutocompleteInput& input,
    size_t prefix_length,
    const std::wstring& remaining_input) {
  DCHECK(model);
  // Get keyword data from data store.
  const TemplateURL* element(model->GetTemplateURLForKeyword(keyword));
  DCHECK(element && element->url());
  const bool supports_replacement = element->url()->SupportsReplacement();

  // Create an edit entry of "[keyword] [remaining input]".  This is helpful
  // even when [remaining input] is empty, as the user can select the popup
  // choice and immediately begin typing in query input.
  const bool keyword_complete = (prefix_length == keyword.length());
  AutocompleteMatch result(this,
      CalculateRelevance(input.type(), keyword_complete,
                         // When the user wants keyword matches to take
                         // preference, score them highly regardless of whether
                         // the input provides query text.
                         input.prefer_keyword() || !supports_replacement),
      false, supports_replacement ? AutocompleteMatch::SEARCH_OTHER_ENGINE :
                                    AutocompleteMatch::HISTORY_KEYWORD);
  result.fill_into_edit.assign(keyword);
  if (!remaining_input.empty() || !keyword_complete || supports_replacement)
    result.fill_into_edit.push_back(L' ');
  result.fill_into_edit.append(remaining_input);
  if (!input.prevent_inline_autocomplete() &&
      (keyword_complete || remaining_input.empty()))
    result.inline_autocomplete_offset = input.text().length();

  // Create destination URL and popup entry content by substituting user input
  // into keyword templates.
  FillInURLAndContents(remaining_input, element, &result);

  // Create popup entry description based on the keyword name.
  result.description.assign(l10n_util::GetStringF(
      IDS_AUTOCOMPLETE_KEYWORD_DESCRIPTION, keyword));
  if (supports_replacement)
    result.template_url = element;
  static const std::wstring kKeywordDesc(l10n_util::GetString(
      IDS_AUTOCOMPLETE_KEYWORD_DESCRIPTION));
  AutocompleteMatch::ClassifyLocationInString(kKeywordDesc.find(L"%s"),
                                              prefix_length,
                                              result.description.length(),
                                              ACMatchClassification::DIM,
                                              &result.description_class);

  result.transition = PageTransition::KEYWORD;

  return result;
}
