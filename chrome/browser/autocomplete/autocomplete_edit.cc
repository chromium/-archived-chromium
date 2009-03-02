// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_edit.h"

#include <locale>

// TODO(deanm): Cleanup these includes.
#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_clipboard_writer.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/autocomplete/autocomplete_accessibility.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view_win.h"
#include "chrome/browser/autocomplete/edit_drop_target.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/drag_utils.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/common/clipboard_service.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/win_util.h"
#include "googleurl/src/url_util.h"
#include "grit/generated_resources.h"
#include "skia/include/SkBitmap.h"

///////////////////////////////////////////////////////////////////////////////
// AutocompleteEditModel

// A single AutocompleteController used solely for making synchronous calls to
// determine how to deal with the clipboard contents for Paste And Go
// functionality.  We avoid using the popup's controller here because we don't
// want to interrupt in-progress queries or modify the popup state just
// because the user right-clicked the edit.  We don't need a controller for
// every edit because this will always be accessed on the main thread, so we
// won't have thread-safety problems.
static AutocompleteController* paste_and_go_controller = NULL;
static int paste_and_go_controller_refcount = 0;

AutocompleteEditModel::AutocompleteEditModel(
    AutocompleteEditView* view,
    AutocompleteEditController* controller,
    Profile* profile)
    : view_(view),
      controller_(controller),
      has_focus_(false),
      user_input_in_progress_(false),
      just_deleted_text_(false),
      has_temporary_text_(false),
      paste_state_(NONE),
      control_key_state_(UP),
      is_keyword_hint_(false),
      keyword_ui_state_(NORMAL),
      show_search_hint_(true),
      profile_(profile) {
  if (++paste_and_go_controller_refcount == 1) {
    // We don't have a controller yet, so create one.  No profile is set since
    // we'll set this before each call to the controller.
    paste_and_go_controller = new AutocompleteController(NULL);
  }
}

AutocompleteEditModel::~AutocompleteEditModel() {
  if (--paste_and_go_controller_refcount == 0)
    delete paste_and_go_controller;
}

void AutocompleteEditModel::SetProfile(Profile* profile) {
  DCHECK(profile);
  profile_ = profile;
  popup_->SetProfile(profile);
}

const AutocompleteEditModel::State
    AutocompleteEditModel::GetStateForTabSwitch() {
  // Like typing, switching tabs "accepts" the temporary text as the user
  // text, because it makes little sense to have temporary text when the
  // popup is closed.
  if (user_input_in_progress_)
    InternalSetUserText(UserTextFromDisplayText(view_->GetText()));

  return State(user_input_in_progress_, user_text_, keyword_, is_keyword_hint_,
      keyword_ui_state_, show_search_hint_);
}

void AutocompleteEditModel::RestoreState(const State& state) {
  // Restore any user editing.
  if (state.user_input_in_progress) {
    // NOTE: Be sure and set keyword-related state BEFORE invoking
    // DisplayTextFromUserText(), as its result depends upon this state.
    keyword_ = state.keyword;
    is_keyword_hint_ = state.is_keyword_hint;
    keyword_ui_state_ = state.keyword_ui_state;
    show_search_hint_ = state.show_search_hint;
    view_->SetUserText(state.user_text,
        DisplayTextFromUserText(state.user_text), false);
  }
}

bool AutocompleteEditModel::UpdatePermanentText(
    const std::wstring& new_permanent_text) {
  // When there's a new URL, and the user is not editing anything or the edit
  // doesn't have focus, we want to revert the edit to show the new URL.  (The
  // common case where the edit doesn't have focus is when the user has started
  // an edit and then abandoned it and clicked a link on the page.)
  const bool visibly_changed_permanent_text =
      (permanent_text_ != new_permanent_text) &&
      (!user_input_in_progress_ || !has_focus_);

  permanent_text_ = new_permanent_text;
  return visibly_changed_permanent_text;
}

void AutocompleteEditModel::SetUserText(const std::wstring& text) {
  SetInputInProgress(true);
  InternalSetUserText(text);
  paste_state_ = NONE;
  has_temporary_text_ = false;
}

void AutocompleteEditModel::GetDataForURLExport(GURL* url,
                                                std::wstring* title,
                                                SkBitmap* favicon) {
  *url = GetURLForCurrentText(NULL, NULL, NULL);
  if (UTF8ToWide(url->possibly_invalid_spec()) == permanent_text_) {
    *title = controller_->GetTitle();
    *favicon = controller_->GetFavIcon();
  }
}

std::wstring AutocompleteEditModel::GetDesiredTLD() const {
  return (control_key_state_ == DOWN_WITHOUT_CHANGE) ?
    std::wstring(L"com") : std::wstring();
}

bool AutocompleteEditModel::CurrentTextIsURL() {
  // If !user_input_in_progress_, the permanent text is showing, which should
  // always be a URL, so no further checking is needed.  By avoiding checking in
  // this case, we avoid calling into the autocomplete providers, and thus
  // initializing the history system, as long as possible, which speeds startup.
  if (!user_input_in_progress_)
    return true;

  PageTransition::Type transition = PageTransition::LINK;
  GetURLForCurrentText(&transition, NULL, NULL);
  return transition == PageTransition::TYPED;
}

bool AutocompleteEditModel::GetURLForText(const std::wstring& text,
                                          GURL* url) const {
  url_parse::Parsed parts;
  const AutocompleteInput::Type type = AutocompleteInput::Parse(
      UserTextFromDisplayText(text), std::wstring(), &parts, NULL);
  if (type != AutocompleteInput::URL)
    return false;
    
  *url = GURL(URLFixerUpper::FixupURL(text, std::wstring()));
  return true;
}

void AutocompleteEditModel::SetInputInProgress(bool in_progress) {
  if (user_input_in_progress_ == in_progress)
    return;

  user_input_in_progress_ = in_progress;
  controller_->OnInputInProgress(in_progress);
}

void AutocompleteEditModel::Revert() {
  SetInputInProgress(false);
  paste_state_ = NONE;
  InternalSetUserText(std::wstring());
  keyword_.clear();
  is_keyword_hint_ = false;
  keyword_ui_state_ = NORMAL;
  show_search_hint_ = permanent_text_.empty();
  has_temporary_text_ = false;
  view_->SetWindowTextAndCaretPos(permanent_text_,
                                  has_focus_ ? permanent_text_.length() : 0);
}

void AutocompleteEditModel::StartAutocomplete(
    bool prevent_inline_autocomplete) const {
  popup_->StartAutocomplete(user_text_, GetDesiredTLD(),
      prevent_inline_autocomplete || just_deleted_text_ ||
      (paste_state_ != NONE), keyword_ui_state_ == KEYWORD);
}

bool AutocompleteEditModel::CanPasteAndGo(const std::wstring& text) const {
  // Reset local state.
  paste_and_go_url_ = GURL();
  paste_and_go_transition_ = PageTransition::TYPED;
  paste_and_go_alternate_nav_url_ = GURL();

  // Ask the controller what do do with this input.
  paste_and_go_controller->SetProfile(profile_);
                              // This is cheap, and since there's one
                              // paste_and_go_controller for many tabs which
                              // may all have different profiles, it ensures
                              // we're always using the right one.
  paste_and_go_controller->Start(text, std::wstring(), true, false, true);
  DCHECK(paste_and_go_controller->done());
  const AutocompleteResult& result = paste_and_go_controller->result();
  if (result.empty())
    return false;

  // Set local state based on the default action for this input.
  const AutocompleteResult::const_iterator match(result.default_match());
  DCHECK(match != result.end());
  paste_and_go_url_ = match->destination_url;
  paste_and_go_transition_ = match->transition;
  paste_and_go_alternate_nav_url_ =
      result.GetAlternateNavURL(paste_and_go_controller->input(), match);

  return paste_and_go_url_.is_valid();
}

void AutocompleteEditModel::PasteAndGo() {
  // The final parameter to OpenURL, keyword, is not quite correct here: it's
  // possible to "paste and go" a string that contains a keyword.  This is
  // enough of an edge case that we ignore this possibility.
  view_->RevertAll();
  view_->OpenURL(paste_and_go_url_, CURRENT_TAB, paste_and_go_transition_,
      paste_and_go_alternate_nav_url_, AutocompletePopupModel::kNoMatch,
      std::wstring());
}

void AutocompleteEditModel::AcceptInput(WindowOpenDisposition disposition,
                                        bool for_drop) {
  // Get the URL and transition type for the selected entry.
  PageTransition::Type transition;
  bool is_history_what_you_typed_match;
  GURL alternate_nav_url;
  const GURL url(GetURLForCurrentText(&transition,
                                      &is_history_what_you_typed_match,
                                      &alternate_nav_url));
  if (!url.is_valid())
    return;

  if (UTF8ToWide(url.spec()) == permanent_text_) {
    // When the user hit enter on the existing permanent URL, treat it like a
    // reload for scoring purposes.  We could detect this by just checking
    // user_input_in_progress_, but it seems better to treat "edits" that end
    // up leaving the URL unchanged (e.g. deleting the last character and then
    // retyping it) as reloads too.
    transition = PageTransition::RELOAD;
  } else if (for_drop || ((paste_state_ != NONE) &&
                          is_history_what_you_typed_match)) {
    // When the user pasted in a URL and hit enter, score it like a link click
    // rather than a normal typed URL, so it doesn't get inline autocompleted
    // as aggressively later.
    transition = PageTransition::LINK;
  }

  view_->OpenURL(url, disposition, transition, alternate_nav_url,
      AutocompletePopupModel::kNoMatch,
      is_keyword_hint_ ? std::wstring() : keyword_);
}

void AutocompleteEditModel::SendOpenNotification(size_t selected_line,
                                                 const std::wstring& keyword) {
  // We only care about cases where there is a selection (i.e. the popup is
  // open).
  if (popup_->IsOpen()) {
    scoped_ptr<AutocompleteLog> log(popup_->GetAutocompleteLog());
    if (selected_line != AutocompletePopupModel::kNoMatch)
      log->selected_index = selected_line;
    else if (!has_temporary_text_)
      log->inline_autocompleted_length = inline_autocomplete_text_.length();
    NotificationService::current()->Notify(
        NotificationType::OMNIBOX_OPENED_URL, Source<Profile>(profile_),
        Details<AutocompleteLog>(log.get()));
  }

  TemplateURLModel* template_url_model = profile_->GetTemplateURLModel();
  if (keyword.empty() || !template_url_model)
    return;

  const TemplateURL* const template_url =
      template_url_model->GetTemplateURLForKeyword(keyword);
  if (template_url) {
    UserMetrics::RecordAction(L"AcceptedKeyword", profile_);
    template_url_model->IncrementUsageCount(template_url);
  }

  // NOTE: We purposefully don't increment the usage count of the default search
  // engine, if applicable; see comments in template_url.h.
}

void AutocompleteEditModel::AcceptKeyword() {
  view_->OnBeforePossibleChange();
  view_->SetWindowTextAndCaretPos(std::wstring(), 0);
  is_keyword_hint_ = false;
  keyword_ui_state_ = KEYWORD;
  view_->OnAfterPossibleChange();
  just_deleted_text_ = false;  // OnAfterPossibleChange() erroneously sets this
                               // since the edit contents have disappeared.  It
                               // doesn't really matter, but we clear it to be
                               // consistent.
  UserMetrics::RecordAction(L"AcceptedKeywordHint", profile_);
}

void AutocompleteEditModel::ClearKeyword(const std::wstring& visible_text) {
  view_->OnBeforePossibleChange();
  const std::wstring window_text(keyword_ + visible_text);
  view_->SetWindowTextAndCaretPos(window_text.c_str(), keyword_.length());
  keyword_.clear();
  keyword_ui_state_ = NORMAL;
  view_->OnAfterPossibleChange();
  just_deleted_text_ = true;  // OnAfterPossibleChange() fails to clear this
                              // since the edit contents have actually grown
                              // longer.
}

bool AutocompleteEditModel::query_in_progress() const {
  return !popup_->autocomplete_controller()->done();
}

const AutocompleteResult& AutocompleteEditModel::result() const {
  return popup_->autocomplete_controller()->result();
}

void AutocompleteEditModel::OnSetFocus(bool control_down) {
  has_focus_ = true;
  control_key_state_ = control_down ? DOWN_WITHOUT_CHANGE : UP;
}

void AutocompleteEditModel::OnKillFocus() {
  has_focus_ = false;
  control_key_state_ = UP;
  paste_state_ = NONE;

  // Like typing, killing focus "accepts" the temporary text as the user
  // text, because it makes little sense to have temporary text when the
  // popup is closed.
  InternalSetUserText(UserTextFromDisplayText(view_->GetText()));
  has_temporary_text_ = false;
}

bool AutocompleteEditModel::OnEscapeKeyPressed() {
  if (has_temporary_text_ &&
      (popup_->URLsForCurrentSelection(NULL, NULL, NULL) != original_url_)) {
    // The user typed something, then selected a different item.  Restore the
    // text they typed and change back to the default item.
    // NOTE: This purposefully does not reset paste_state_.
    just_deleted_text_ = false;
    has_temporary_text_ = false;
    keyword_ui_state_ = original_keyword_ui_state_;
    popup_->ResetToDefaultMatch();
    view_->OnRevertTemporaryText();
    return true;
  }

  // If the user wasn't editing, but merely had focus in the edit, allow <esc> 
  // to be processed as an accelerator, so it can still be used to stop a load. 
  // When the permanent text isn't all selected we still fall through to the 
  // SelectAll() call below so users can arrow around in the text and then hit 
  // <esc> to quickly replace all the text; this matches IE.
  if (!user_input_in_progress_ && view_->IsSelectAll()) 
    return false;

  view_->RevertAll();
  view_->SelectAll(true);
  return false;
}

void AutocompleteEditModel::OnControlKeyChanged(bool pressed) {
  // Don't change anything unless the key state is actually toggling.
  if (pressed == (control_key_state_ == UP)) {
    control_key_state_ = pressed ? DOWN_WITHOUT_CHANGE : UP;
    if (popup_->IsOpen()) {
      // Autocomplete history provider results may change, so refresh the
      // popup.  This will force user_input_in_progress_ to true, but if the
      // popup is open, that should have already been the case.
      view_->UpdatePopup();
    }
  }
}

void AutocompleteEditModel::OnUpOrDownKeyPressed(int count) {
  // NOTE: This purposefully don't trigger any code that resets paste_state_.

  if (!popup_->IsOpen()) {
    if (popup_->autocomplete_controller()->done()) {
      // The popup is neither open nor working on a query already.  So, start an
      // autocomplete query for the current text.  This also sets
      // user_input_in_progress_ to true, which we want: if the user has started
      // to interact with the popup, changing the permanent_text_ shouldn't
      // change the displayed text.
      // Note: This does not force the popup to open immediately.
      // TODO(pkasting): We should, in fact, force this particular query to open
      // the popup immediately.
      if (!user_input_in_progress_)
        InternalSetUserText(permanent_text_);
      view_->UpdatePopup();
    } else {
      // TODO(pkasting): The popup is working on a query but is not open.  We
      // should force it to open immediately.
    }
  } else {
    // The popup is open, so the user should be able to interact with it
    // normally.
    popup_->Move(count);
  }

  // NOTE: We need to reset the keyword_ui_state_ after the popup updates, since
  // Move() will eventually call back to OnPopupDataChanged(), which needs to
  // save off the current keyword_ui_state_.
  keyword_ui_state_ = NORMAL;
}

void AutocompleteEditModel::OnPopupDataChanged(
    const std::wstring& text,
    bool is_temporary_text,
    const std::wstring& keyword,
    bool is_keyword_hint,
    AutocompleteMatch::Type type) {
  // We don't want to show the search hint if we're showing a keyword hint or
  // selected keyword, or (subtle!) if we would be showing a selected keyword
  // but for keyword_ui_state_ == NO_KEYWORD.
  const bool show_search_hint = keyword.empty() &&
      ((type == AutocompleteMatch::SEARCH_WHAT_YOU_TYPED) ||
       (type == AutocompleteMatch::SEARCH_HISTORY) ||
       (type == AutocompleteMatch::SEARCH_SUGGEST));

  // Update keyword/hint-related local state.
  bool keyword_state_changed = (keyword_ != keyword) ||
      ((is_keyword_hint_ != is_keyword_hint) && !keyword.empty()) ||
      (show_search_hint_ != show_search_hint);
  if (keyword_state_changed) {
    keyword_ = keyword;
    is_keyword_hint_ = is_keyword_hint;
    show_search_hint_ = show_search_hint;
  }

  // Handle changes to temporary text.
  if (is_temporary_text) {
    const bool save_original_selection = !has_temporary_text_;
    if (save_original_selection) {
      // Save the original selection and URL so it can be reverted later.
      has_temporary_text_ = true;
      original_url_ = popup_->URLsForCurrentSelection(NULL, NULL, NULL);
      original_keyword_ui_state_ = keyword_ui_state_;
    }
    view_->OnTemporaryTextMaybeChanged(DisplayTextFromUserText(text),
                                       save_original_selection);
    return;
  }

  // Handle changes to inline autocomplete text.  Don't make changes if the user
  // is showing temporary text.  Making display changes would be obviously
  // wrong; making changes to the inline_autocomplete_text_ itself turns out to
  // be more subtlely wrong, because it means hitting esc will no longer revert
  // to the original state before arrowing.
  if (!has_temporary_text_) {
    inline_autocomplete_text_ = text;
    if (view_->OnInlineAutocompleteTextMaybeChanged(
        DisplayTextFromUserText(user_text_ + inline_autocomplete_text_),
        DisplayTextFromUserText(user_text_).length()))
      return;
  }

  // If the above changes didn't warrant a text update but we did change keyword
  // state, we have yet to notify the controller about it.
  if (keyword_state_changed)
    controller_->OnChanged();
}

bool AutocompleteEditModel::OnAfterPossibleChange(const std::wstring& new_text,
                                                  bool selection_differs,
                                                  bool text_differs,
                                                  bool just_deleted_text,
                                                  bool at_end_of_edit) {
  // Update the paste state as appropriate: if we're just finishing a paste
  // that replaced all the text, preserve that information; otherwise, if we've
  // made some other edit, clear paste tracking.
  if (paste_state_ == REPLACING_ALL)
    paste_state_ = REPLACED_ALL;
  else if (text_differs)
    paste_state_ = NONE;

  // If something has changed while the control key is down, prevent
  // "ctrl-enter" until the control key is released.  When we do this, we need
  // to update the popup if it's open, since the desired_tld will have changed.
  if ((text_differs || selection_differs) &&
      (control_key_state_ == DOWN_WITHOUT_CHANGE)) {
    control_key_state_ = DOWN_WITH_CHANGE;
    if (!text_differs && !popup_->IsOpen())
      return false;  // Don't open the popup for no reason.
  } else if (!text_differs &&
             (inline_autocomplete_text_.empty() || !selection_differs)) {
    return false;
  }

  const bool had_keyword = (keyword_ui_state_ != NO_KEYWORD) &&
      !is_keyword_hint_ && !keyword_.empty();

  // Modifying the selection counts as accepting the autocompleted text.
  InternalSetUserText(UserTextFromDisplayText(new_text));
  has_temporary_text_ = false;

  // Track when the user has deleted text so we won't allow inline autocomplete.
  just_deleted_text_ = just_deleted_text;

  // Disable the fancy keyword UI if the user didn't already have a visible
  // keyword and is not at the end of the edit.  This prevents us from showing
  // the fancy UI (and interrupting the user's editing) if the user happens to
  // have a keyword for 'a', types 'ab' then puts a space between the 'a' and
  // the 'b'.
  if (!had_keyword)
    keyword_ui_state_ = at_end_of_edit ? NORMAL : NO_KEYWORD;

  view_->UpdatePopup();

  if (had_keyword) {
    if (is_keyword_hint_ || keyword_.empty())
      keyword_ui_state_ = NORMAL;
  } else if ((keyword_ui_state_ != NO_KEYWORD) && !is_keyword_hint_ &&
             !keyword_.empty()) {
    // Went from no selected keyword to a selected keyword.
    keyword_ui_state_ = KEYWORD;
  }

  return true;
}

void AutocompleteEditModel::InternalSetUserText(const std::wstring& text) {
  user_text_ = text;
  just_deleted_text_ = false;
  inline_autocomplete_text_.clear();
}

std::wstring AutocompleteEditModel::DisplayTextFromUserText(
    const std::wstring& text) const {
  return ((keyword_ui_state_ == NO_KEYWORD) || is_keyword_hint_ ||
          keyword_.empty()) ?
      text : KeywordProvider::SplitReplacementStringFromInput(text);
}

std::wstring AutocompleteEditModel::UserTextFromDisplayText(
    const std::wstring& text) const {
  return ((keyword_ui_state_ == NO_KEYWORD) || is_keyword_hint_ ||
          keyword_.empty()) ?
      text : (keyword_ + L" " + text);
}

GURL AutocompleteEditModel::GetURLForCurrentText(
    PageTransition::Type* transition,
    bool* is_history_what_you_typed_match,
    GURL* alternate_nav_url) {
  return (popup_->IsOpen() || !popup_->autocomplete_controller()->done()) ?
      popup_->URLsForCurrentSelection(transition,
                                      is_history_what_you_typed_match,
                                      alternate_nav_url) :
      popup_->URLsForDefaultMatch(UserTextFromDisplayText(view_->GetText()),
                                  GetDesiredTLD(), transition,
                                  is_history_what_you_typed_match,
                                  alternate_nav_url);
}
