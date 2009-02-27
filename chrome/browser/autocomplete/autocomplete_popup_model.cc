// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_popup_model.h"

// TODO(deanm): Clean up these includes, not going to fight it now.
#include <cmath>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/resource_bundle.h"
#include "grit/theme_resources.h"
#include "third_party/icu38/public/common/unicode/ubidi.h"

AutocompletePopupModel::AutocompletePopupModel(
    const ChromeFont& font,
    AutocompleteEditView* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile)
    : view_(new AutocompletePopupView(this, font, edit_view)),
      edit_model_(edit_model),
      controller_(new AutocompleteController(profile)),
      profile_(profile),
      hovered_line_(kNoMatch),
      selected_line_(kNoMatch),
      inside_synchronous_query_(false) {
  registrar_.Add(
      this,
      NotificationType::AUTOCOMPLETE_CONTROLLER_RESULT_UPDATED,
      Source<AutocompleteController>(controller_.get()));
  registrar_.Add(
      this,
      NotificationType::AUTOCOMPLETE_CONTROLLER_SYNCHRONOUS_MATCHES_AVAILABLE,
      Source<AutocompleteController>(controller_.get()));
}

AutocompletePopupModel::~AutocompletePopupModel() {
  StopAutocomplete();
}

void AutocompletePopupModel::SetProfile(Profile* profile) {
  DCHECK(profile);
  profile_ = profile;
  controller_->SetProfile(profile);
}

void AutocompletePopupModel::StartAutocomplete(
    const std::wstring& text,
    const std::wstring& desired_tld,
    bool prevent_inline_autocomplete,
    bool prefer_keyword) {
  // The user is interacting with the edit, so stop tracking hover.
  SetHoveredLine(kNoMatch);

  manually_selected_match_.Clear();

  controller_->Start(text, desired_tld, prevent_inline_autocomplete,
                     prefer_keyword, false);
}

void AutocompletePopupModel::StopAutocomplete() {
  controller_->Stop(true);
  SetHoveredLine(kNoMatch);
  selected_line_ = kNoMatch;
  view_->UpdatePopupAppearance();
}

bool AutocompletePopupModel::IsOpen() const {
  return view_->IsOpen();
}

void AutocompletePopupModel::SetHoveredLine(size_t line) {
  const bool is_disabling = (line == kNoMatch);
  DCHECK(is_disabling || (line < controller_->result().size()));

  if (line == hovered_line_)
    return;  // Nothing to do

  // Make sure the old hovered line is redrawn.  No need to redraw the selected
  // line since selection overrides hover so the appearance won't change.
  const bool is_enabling = (hovered_line_ == kNoMatch);
  if (!is_enabling && (hovered_line_ != selected_line_))
    view_->InvalidateLine(hovered_line_);

  // Change the hover to the new line and make sure it's redrawn.
  hovered_line_ = line;
  if (!is_disabling && (hovered_line_ != selected_line_))
    view_->InvalidateLine(hovered_line_);

  if (is_enabling || is_disabling)
    view_->OnHoverEnabledOrDisabled(is_disabling);
}

void AutocompletePopupModel::SetSelectedLine(size_t line,
                                             bool reset_to_default) {
  const AutocompleteResult& result = controller_->result();
  DCHECK(line < result.size());
  if (result.empty())
    return;

  // Cancel the query so the matches don't change on the user.
  controller_->Stop(false);

  const AutocompleteMatch& match = result.match_at(line);
  if (reset_to_default) {
    manually_selected_match_.Clear();
  } else {
    // Track the user's selection until they cancel it.
    manually_selected_match_.destination_url = match.destination_url;
    manually_selected_match_.provider_affinity = match.provider;
    manually_selected_match_.is_history_what_you_typed_match =
        match.is_history_what_you_typed_match;
  }

  if (line == selected_line_)
    return;  // Nothing else to do.

  // Update the edit with the new data for this match.
  std::wstring keyword;
  const bool is_keyword_hint = GetKeywordForMatch(match, &keyword);
  edit_model_->OnPopupDataChanged(
      reset_to_default ? std::wstring() : match.fill_into_edit,
      !reset_to_default, keyword, is_keyword_hint, match.type);

  // Repaint old and new selected lines immediately, so that the edit doesn't
  // appear to update [much] faster than the popup.  We must not update
  // |selected_line_| before calling OnPopupDataChanged() (since the edit may
  // call us back to get data about the old selection), and we must not call
  // UpdateWindow() before updating |selected_line_| (since the paint routine
  // relies on knowing the correct selected line).
  view_->InvalidateLine(selected_line_);
  selected_line_ = line;
  view_->InvalidateLine(selected_line_);
  view_->UpdateWindow();
}

void AutocompletePopupModel::ResetToDefaultMatch() {
  const AutocompleteResult& result = controller_->result();
  DCHECK(!result.empty());
  SetSelectedLine(result.default_match() - result.begin(), true);
}

GURL AutocompletePopupModel::URLsForCurrentSelection(
    PageTransition::Type* transition,
    bool* is_history_what_you_typed_match,
    GURL* alternate_nav_url) const {
  // We need to use the result on the controller, because if the popup is open,
  // the user changes the contents of the edit, and then presses enter before
  // any results have been displayed, results_ will be nonempty but wrong.  (In
  // most other cases, the controller's results will match the popup's.)
  // TODO(pkasting): If manually_selected_match_ moves to the controller, this
  // can move to the edit.
  if (controller_->result().empty())
    return GURL();

  const AutocompleteResult& result = controller_->result();
  AutocompleteResult::const_iterator match;
  if (!controller_->done()) {
    // The user cannot have manually selected a match, or the query would have
    // stopped.  So the default match must be the desired selection.
    match = result.default_match();
  } else {
    // The query isn't running, so the popup can't possibly be out of date.
    DCHECK(selected_line_ < result.size());
    match = result.begin() + selected_line_;
  }
  if (transition)
    *transition = match->transition;
  if (is_history_what_you_typed_match)
    *is_history_what_you_typed_match = match->is_history_what_you_typed_match;
  if (alternate_nav_url && manually_selected_match_.empty())
    *alternate_nav_url = result.GetAlternateNavURL(controller_->input(), match);
  return match->destination_url;
}

GURL AutocompletePopupModel::URLsForDefaultMatch(
    const std::wstring& text,
    const std::wstring& desired_tld,
    PageTransition::Type* transition,
    bool* is_history_what_you_typed_match,
    GURL* alternate_nav_url) {
  // We had better not already be doing anything, or this call will blow it
  // away.
  DCHECK(!IsOpen());
  DCHECK(controller_->done());

  // Run the new query and get only the synchronously available matches.
  inside_synchronous_query_ = true;  // Tell Observe() not to notify the edit or
                                     // update our appearance.
  controller_->Start(text, desired_tld, true, false, true);
  inside_synchronous_query_ = false;
  DCHECK(controller_->done());
  const AutocompleteResult& result = controller_->result();
  if (result.empty())
    return GURL();

  // Get the URLs for the default match.
  const AutocompleteResult::const_iterator match = result.default_match();
  if (transition)
    *transition = match->transition;
  if (is_history_what_you_typed_match)
    *is_history_what_you_typed_match = match->is_history_what_you_typed_match;
  if (alternate_nav_url)
    *alternate_nav_url = result.GetAlternateNavURL(controller_->input(), match);
  return match->destination_url;
}

bool AutocompletePopupModel::GetKeywordForMatch(const AutocompleteMatch& match,
                                                std::wstring* keyword) const {
  // Assume we have no keyword until we find otherwise.
  keyword->clear();

  // If the current match is a keyword, return that as the selected keyword.
  if (match.template_url && match.template_url->url() &&
      match.template_url->url()->SupportsReplacement()) {
    keyword->assign(match.template_url->keyword());
    return false;
  }

  // See if the current match's fill_into_edit corresponds to a keyword.
  if (!profile_->GetTemplateURLModel())
    return false;
  profile_->GetTemplateURLModel()->Load();
  const std::wstring keyword_hint(
      TemplateURLModel::CleanUserInputKeyword(match.fill_into_edit));
  if (keyword_hint.empty())
    return false;

  // Don't provide a hint if this keyword doesn't support replacement.
  const TemplateURL* const template_url =
      profile_->GetTemplateURLModel()->GetTemplateURLForKeyword(keyword_hint);
  if (!template_url || !template_url->url() ||
      !template_url->url()->SupportsReplacement())
    return false;

  keyword->assign(keyword_hint);
  return true;
}

AutocompleteLog* AutocompletePopupModel::GetAutocompleteLog() {
  return new AutocompleteLog(controller_->input().text(),
      controller_->input().type(), selected_line_, 0, controller_->result());
}

void AutocompletePopupModel::Move(int count) {
  // TODO(pkasting): Temporary hack.  If the query is running while the popup is
  // open, we might be showing the results of the previous query still.  Force
  // the popup to display the latest results so the popup and the controller
  // aren't out of sync.  The better fix here is to roll the controller back to
  // be in sync with what the popup is showing.
  if (IsOpen() && !controller_->done()) {
    Observe(NotificationType::AUTOCOMPLETE_CONTROLLER_RESULT_UPDATED,
            Source<AutocompleteController>(controller_.get()),
            NotificationService::NoDetails());
  }

  const AutocompleteResult& result = controller_->result();
  if (result.empty())
    return;

  // The user is using the keyboard to change the selection, so stop tracking
  // hover.
  SetHoveredLine(kNoMatch);

  // Clamp the new line to [0, result_.count() - 1].
  const size_t new_line = selected_line_ + count;
  SetSelectedLine((((count < 0) && (new_line >= selected_line_)) ?
      0 : std::min(new_line, result.size() - 1)), false);
}

void AutocompletePopupModel::TryDeletingCurrentItem() {
  // We could use URLsForCurrentSelection() here, but it seems better to try
  // and shift-delete the actual selection, rather than any "in progress, not
  // yet visible" one.
  if (selected_line_ == kNoMatch)
    return;
  const AutocompleteMatch& match =
      controller_->result().match_at(selected_line_);
  if (match.deletable) {
    const size_t selected_line = selected_line_;
    controller_->DeleteMatch(match);  // This will synchronously notify us that
                                      // the results have changed.
    const AutocompleteResult& result = controller_->result();
    if (!result.empty()) {
      // Move the selection to the next choice after the deleted one.
      // TODO(pkasting): Eventually the controller should take care of this
      // before notifying us, reducing flicker.  At that point the check for
      // deletability can move there too.
      SetSelectedLine(std::min(result.size() - 1, selected_line), false);
    }
  }
}

void AutocompletePopupModel::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (inside_synchronous_query_)
    return;

  const AutocompleteResult& result = controller_->result();
  switch (type.value) {
    case NotificationType::AUTOCOMPLETE_CONTROLLER_RESULT_UPDATED: {
      selected_line_ = (result.default_match() == result.end()) ?
          kNoMatch : (result.default_match() - result.begin());
      // If we're going to trim the window size to no longer include the hovered
      // line, turn hover off.  Practically, this shouldn't happen, but it
      // doesn't hurt to be defensive.
      if ((hovered_line_ != kNoMatch) && (result.size() <= hovered_line_))
        SetHoveredLine(kNoMatch);

      view_->UpdatePopupAppearance();
    }
    // FALL THROUGH

    case NotificationType::AUTOCOMPLETE_CONTROLLER_SYNCHRONOUS_MATCHES_AVAILABLE: {
      // Update the edit with the possibly new data for this match.
      // NOTE: This must be done after the code above, so that our internal
      // state will be consistent when the edit calls back to
      // URLsForCurrentSelection().
      std::wstring inline_autocomplete_text;
      std::wstring keyword;
      bool is_keyword_hint = false;
      AutocompleteMatch::Type type = AutocompleteMatch::SEARCH_WHAT_YOU_TYPED;
      const AutocompleteResult::const_iterator match(result.default_match());
      if (match != result.end()) {
        if ((match->inline_autocomplete_offset != std::wstring::npos) &&
            (match->inline_autocomplete_offset <
                match->fill_into_edit.length())) {
          inline_autocomplete_text =
              match->fill_into_edit.substr(match->inline_autocomplete_offset);
        }
        // Warm up DNS Prefetch Cache.
        chrome_browser_net::DnsPrefetchUrl(match->destination_url);
        // We could prefetch the alternate nav URL, if any, but because there
        // can be many of these as a user types an initial series of characters,
        // the OS DNS cache could suffer eviction problems for minimal gain.

        is_keyword_hint = GetKeywordForMatch(*match, &keyword);
        type = match->type;
      }
      edit_model_->OnPopupDataChanged(inline_autocomplete_text, false, keyword,
          is_keyword_hint, type);
      return;
    }

    default:
      NOTREACHED();
  }
}
