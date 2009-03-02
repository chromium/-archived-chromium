// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_H_

#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompletePopupModel;
class Profile;
class SkBitmap;

class AutocompleteEditController;
class AutocompleteEditModel;
class AutocompleteEditView;

// TODO(pkasting): http://b/1343512  The names and contents of the classes in
// this file are temporary.  I am in hack-and-slash mode right now.

// Embedders of an AutocompleteEdit widget must implement this class.
class AutocompleteEditController {
 public:
  // When the user presses enter or selects a line with the mouse, this
  // function will get called synchronously with the url to open and
  // disposition and transition to use when opening it.
  //
  // |alternate_nav_url|, if non-empty, contains the alternate navigation URL
  // for |url|, which the controller can check for existence.  See comments on
  // AutocompleteResult::GetAlternateNavURL().
  virtual void OnAutocompleteAccept(const GURL& url,
      WindowOpenDisposition disposition,
      PageTransition::Type transition,
      const GURL& alternate_nav_url) = 0;

  // Called when anything has changed that might affect the layout or contents
  // of the views around the edit, including the text of the edit and the
  // status of any keyword- or hint-related state.
  virtual void OnChanged() = 0;

  // Called whenever the user starts or stops an input session (typing,
  // interacting with the edit, etc.).  When user input is not in progress,
  // the edit is guaranteed to be showing the permanent text.
  virtual void OnInputInProgress(bool in_progress) = 0;

  // Returns the favicon of the current page.
  virtual SkBitmap GetFavIcon() const = 0;

  // Returns the title of the current page.
  virtual std::wstring GetTitle() const = 0;
};

class AutocompleteEditModel {
 public:
  enum KeywordUIState {
    // The user is typing normally.
    NORMAL,
    // The user is editing in the middle of the input string.  Even if the
    // input looks like a keyword, don't display the keyword UI, as to not
    // interfere with the user's editing.
    NO_KEYWORD,
    // The user has triggered the keyword UI.  Until it disappears, bias
    // autocomplete results so that input strings of the keyword alone default
    // to the keyword provider, not a normal navigation or search.
    KEYWORD,
  };

  struct State {
    State(bool user_input_in_progress,
          const std::wstring& user_text,
          const std::wstring& keyword,
          bool is_keyword_hint,
          KeywordUIState keyword_ui_state,
          bool show_search_hint)
        : user_input_in_progress(user_input_in_progress),
          user_text(user_text),
          keyword(keyword),
          is_keyword_hint(is_keyword_hint),
          keyword_ui_state(keyword_ui_state),
          show_search_hint(show_search_hint) {
    }

    bool user_input_in_progress;
    const std::wstring user_text;
    const std::wstring keyword;
    const bool is_keyword_hint;
    const KeywordUIState keyword_ui_state;
    const bool show_search_hint;
  };

  AutocompleteEditModel(AutocompleteEditView* view,
                        AutocompleteEditController* controller,
                        Profile* profile);
  ~AutocompleteEditModel();

  void set_popup_model(AutocompletePopupModel* popup_model) {
    popup_ = popup_model;
  }

  // Invoked when the profile has changed.
  void SetProfile(Profile* profile);

  Profile* profile() const { return profile_; }

  // Returns the current state.  This assumes we are switching tabs, and changes
  // the internal state appropriately.
  const State GetStateForTabSwitch();

  // Restores local state from the saved |state|.
  void RestoreState(const State& state);

  // Called when the user wants to export the entire current text as a URL.
  // Sets the url, and if known, the title and favicon.
  void GetDataForURLExport(GURL* url, std::wstring* title, SkBitmap* favicon);

  // If the user presses ctrl-enter, it means "add .com to the the end".  The
  // desired TLD is the TLD the user desires to add to the end of the current
  // input, if any, based on their control key state and any other actions
  // they've taken.
  std::wstring GetDesiredTLD() const;

  // Returns true if the current edit contents will be treated as a
  // URL/navigation, as opposed to a search.
  bool CurrentTextIsURL();

  // Returns true if |text| (which is display text in the current context)
  // parses as a URL, and in that case sets |url| to the calculated URL.
  // Subtle note: This ignores the desired_tld_ (unlike GetDataForURLExport()
  // and CurrentTextIsURL()).  The view needs this because it calls this
  // function during copy handling, when the control key is down to trigger the
  // copy.
  bool GetURLForText(const std::wstring& text, GURL* url) const;

  bool user_input_in_progress() const { return user_input_in_progress_; }

  // Sets the state of user_input_in_progress_, and notifies the observer if
  // that state has changed.
  void SetInputInProgress(bool in_progress);

  // Updates permanent_text_ to |new_permanent_text|.  Returns true if this
  // change should be immediately user-visible, because either the user is not
  // editing or the edit does not have focus.
  bool UpdatePermanentText(const std::wstring& new_permanent_text);

  // Sets the user_text_ to |text|.  Only the View should call this.
  void SetUserText(const std::wstring& text);

  // Reverts the edit model back to its unedited state (permanent text showing,
  // no user input in progress).
  void Revert();

  // Directs the popup to start autocomplete.
  void StartAutocomplete(bool prevent_inline_autocomplete) const;

  // Determines whether the user can "paste and go", given the specified text.
  // This also updates the internal paste-and-go-related state variables as
  // appropriate so that the controller doesn't need to be repeatedly queried
  // for the same text in every clipboard-related function.
  bool CanPasteAndGo(const std::wstring& text) const;

  // Navigates to the destination last supplied to CanPasteAndGo.
  void PasteAndGo();

  // Returns true if this is a paste-and-search rather than paste-and-go (or
  // nothing).
  bool is_paste_and_search() const {
    return (paste_and_go_transition_ != PageTransition::TYPED);
  }

  // Asks the browser to load the popup's currently selected item, using the
  // supplied disposition.  This may close the popup. If |for_drop| is true,
  // it indicates the input is being accepted as part of a drop operation and
  // the transition should be treated as LINK (so that it won't trigger the
  // URL to be autocompleted).
  void AcceptInput(WindowOpenDisposition disposition,
                   bool for_drop);

  // As necessary, sends out notification that the user is accepting a URL in
  // the edit.  If the accepted URL is from selecting a keyword, |keyword| is
  // the selected keyword.
  // If |selected_line| is kNoMatch, the currently selected line is used for the
  // metrics log record; otherwise, the provided value is used as the selected
  // line.  This is used when the user opens a URL without actually selecting
  // its entry, such as middle-clicking it.
  void SendOpenNotification(size_t selected_line, const std::wstring& keyword);

  bool has_focus() const { return has_focus_; }

  // Accessors for keyword-related state (see comments on keyword_ and
  // is_keyword_hint_).
  std::wstring keyword() const {
    return (is_keyword_hint_ ? has_focus_ : (keyword_ui_state_ != NO_KEYWORD)) ?
        keyword_ : std::wstring();
  }
  bool is_keyword_hint() const { return is_keyword_hint_; }

  // Accepts the current keyword hint as a keyword.
  void AcceptKeyword();

  // Clears the current keyword.  |visible_text| is the (non-keyword) text
  // currently visible in the edit.
  void ClearKeyword(const std::wstring& visible_text);

  // True if we should show the "Type to search" hint (see comments on
  // show_search_hint_).
  bool show_search_hint() const { return has_focus_ && show_search_hint_; }

  // Returns true if a query to an autocomplete provider is currently
  // in progress.  This logic should in the future live in
  // AutocompleteController but resides here for now.  This method is used by
  // AutomationProvider::AutocompleteEditIsQueryInProgress.
  bool query_in_progress() const;

  // Returns the current autocomplete result.  This logic should in the future
  // live in AutocompleteController but resides here for now.  This method is
  // used by AutomationProvider::AutocompleteEditGetMatches.
  const AutocompleteResult& result() const;

  // Called when the view is gaining focus.  |control_down| is whether the
  // control key is down (at the time we're gaining focus).
  void OnSetFocus(bool control_down);

  // Called when the view is losing focus.  Resets some state.
  void OnKillFocus();

  // Called when the user presses the escape key.  Decides what, if anything, to
  // revert about any current edits.  Returns whether the key was handled.
  bool OnEscapeKeyPressed();

  // Called when the user presses or releases the control key.  Changes state as
  // necessary.
  void OnControlKeyChanged(bool pressed);

  // Called when the user pastes in text that replaces the entire edit contents.
  void on_paste_replacing_all() { paste_state_ = REPLACING_ALL; }

  // Called when the user presses up or down.  |count| is a repeat count,
  // negative for moving up, positive for moving down.
  void OnUpOrDownKeyPressed(int count);

  // Called back by the AutocompletePopupModel when any relevant data changes.
  // This rolls together several separate pieces of data into one call so we can
  // update all the UI efficiently:
  //   |text| is either the new temporary text (if |is_temporary_text| is true)
  //     from the user manually selecting a different match, or the inline
  //     autocomplete text (if |is_temporary_text| is false).
  //   |keyword| is the keyword to show a hint for if |is_keyword_hint| is true,
  //     or the currently selected keyword if |is_keyword_hint| is false (see
  //     comments on keyword_ and is_keyword_hint_).
  //   |type| is the type of match selected; this is used to determine whether
  //     we can show the "Type to search" hint (see comments on
  //     show_search_hint_).
  void OnPopupDataChanged(
      const std::wstring& text,
      bool is_temporary_text,
      const std::wstring& keyword,
      bool is_keyword_hint,
      AutocompleteMatch::Type type);

  // Called by the AutocompleteEditView after something changes, with details
  // about what state changes occured.  Updates internal state, updates the
  // popup if necessary, and returns true if any significant changes occurred.
  bool OnAfterPossibleChange(const std::wstring& new_text,
                             bool selection_differs,
                             bool text_differs,
                             bool just_deleted_text,
                             bool at_end_of_edit);

 private:
  enum PasteState {
    NONE,           // Most recent edit was not a paste that replaced all text.
    REPLACED_ALL,   // Most recent edit was a paste that replaced all text.
    REPLACING_ALL,  // In the middle of doing a paste that replaces all
                    // text.  We need this intermediate state because OnPaste()
                    // does the actual detection of such pastes, but
                    // OnAfterPossibleChange() has to update the paste state
                    // for every edit.  If OnPaste() set the state directly to
                    // REPLACED_ALL, OnAfterPossibleChange() wouldn't know
                    // whether that represented the current edit or a past one.
  };

  enum ControlKeyState {
    UP,                   // The control key is not depressed.
    DOWN_WITHOUT_CHANGE,  // The control key is depressed, and the edit's
                          // contents/selection have not changed since it was
                          // depressed.  This is the only state in which we
                          // do the "ctrl-enter" behavior when the user hits
                          // enter.
    DOWN_WITH_CHANGE,     // The control key is depressed, and the edit's
                          // contents/selection have changed since it was
                          // depressed.  If the user now hits enter, we assume
                          // he simply hasn't released the key, rather than that
                          // he intended to hit "ctrl-enter".
  };

  // Called whenever user_text_ should change.
  void InternalSetUserText(const std::wstring& text);

  // Conversion between user text and display text. User text is the text the
  // user has input. Display text is the text being shown in the edit. The
  // two are different if a keyword is selected.
  std::wstring DisplayTextFromUserText(const std::wstring& text) const;
  std::wstring UserTextFromDisplayText(const std::wstring& text) const;

  // Returns the URL. If the user has not edited the text, this returns the
  // permanent text. If the user has edited the text, this returns the default
  // match based on the current text, which may be a search URL, or keyword
  // generated URL.
  //
  // See AutocompleteEdit for a description of the args (they may be null if
  // not needed).
  GURL GetURLForCurrentText(PageTransition::Type* transition,
                            bool* is_history_what_you_typed_match,
                            GURL* alternate_nav_url);

  AutocompleteEditView* view_;

  AutocompletePopupModel* popup_;

  AutocompleteEditController* controller_;

  // Whether the edit has focus.
  bool has_focus_;

  // The URL of the currently displayed page.
  std::wstring permanent_text_;

  // This flag is true when the user has modified the contents of the edit, but
  // not yet accepted them.  We use this to determine when we need to save
  // state (on switching tabs) and whether changes to the page URL should be
  // immediately displayed.
  // This flag will be true in a superset of the cases where the popup is open.
  bool user_input_in_progress_;

  // The text that the user has entered.  This does not include inline
  // autocomplete text that has not yet been accepted.
  std::wstring user_text_;

  // When the user closes the popup, we need to remember the URL for their
  // desired choice, so that if they hit enter without reopening the popup we
  // know where to go.  We could simply rerun autocomplete in this case, but
  // we'd need to either wait for all results to come in (unacceptably slow) or
  // do the wrong thing when the user had chosen some provider whose results
  // were not returned instantaneously.
  //
  // This variable is only valid when user_input_in_progress_ is true, since
  // when it is false the user has either never input anything (so there won't
  // be a value here anyway) or has canceled their input, which should be
  // treated the same way.  Also, since this is for preserving a desired URL
  // after the popup has been closed, we ignore this if the popup is open, and
  // simply ask the popup for the desired URL directly.  As a result, the
  // contents of this variable only need to be updated when the popup is closed
  // but user_input_in_progress_ is not being cleared.
  std::wstring url_for_remembered_user_selection_;

  // Inline autocomplete is allowed if the user has not just deleted text, and
  // no temporary text is showing.  In this case, inline_autocomplete_text_ is
  // appended to the user_text_ and displayed selected (at least initially).
  //
  // NOTE: When the popup is closed there should never be inline autocomplete
  // text (actions that close the popup should either accept the text, convert
  // it to a normal selection, or change the edit entirely).
  bool just_deleted_text_;
  std::wstring inline_autocomplete_text_;

  // Used by OnPopupDataChanged to keep track of whether there is currently a
  // temporary text.
  //
  // Example of use: If the user types "goog", then arrows down in the
  // autocomplete popup until, say, "google.com" appears in the edit box, then
  // the user_text_ is still "goog", and "google.com" is "temporary text".
  // When the user hits <esc>, the edit box reverts to "goog".  Hit <esc> again
  // and the popup is closed and "goog" is replaced by the permanent_text_,
  // which is the URL of the current page.
  //
  // original_url_ is only valid when there is temporary text, and is used as
  // the unique identifier of the originally selected item.  Thus, if the user
  // arrows to a different item with the same text, we can still distinguish
  // them and not revert all the way to the permanent_text_.
  bool has_temporary_text_;
  GURL original_url_;
  KeywordUIState original_keyword_ui_state_;

  // When the user's last action was to paste and replace all the text, we
  // disallow inline autocomplete (on the theory that the user is trying to
  // paste in a new URL or part of one, and in either case inline autocomplete
  // would get in the way).
  PasteState paste_state_;

  // Whether the control key is depressed.  We track this to avoid calling
  // UpdatePopup() repeatedly if the user holds down the key, and to know
  // whether to trigger "ctrl-enter" behavior.
  ControlKeyState control_key_state_;

  // The keyword associated with the current match.  The user may have an actual
  // selected keyword, or just some input text that looks like a keyword (so we
  // can show a hint to press <tab>).  This is the keyword in either case;
  // is_keyword_hint_ (below) distinguishes the two cases.
  std::wstring keyword_;

  // True if the keyword associated with this match is merely a hint, i.e. the
  // user hasn't actually selected a keyword yet.  When this is true, we can use
  // keyword_ to show a "Press <tab> to search" sort of hint.
  bool is_keyword_hint_;

  // See KeywordUIState enum.
  KeywordUIState keyword_ui_state_;

  // True when it's safe to show a "Type to search" hint to the user (when the
  // edit is empty, or the user is in the process of searching).
  bool show_search_hint_;

  // Paste And Go-related state.  See CanPasteAndGo().
  mutable GURL paste_and_go_url_;
  mutable PageTransition::Type paste_and_go_transition_;
  mutable GURL paste_and_go_alternate_nav_url_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteEditModel);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_H_
