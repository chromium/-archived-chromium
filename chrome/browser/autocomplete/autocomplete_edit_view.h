// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the interface class AutocompleteEditView.  Each toolkit
// will implement the edit view differently, so that code is inheriently
// platform specific.  However, the AutocompleteEditModel needs to do some
// communication with the view.  Since the model is shared between platforms,
// we need to define an interface that all view implementations will share.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_H_

#include <string>

#include "chrome/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompleteEditModel;
class GURL;
class TabContents;

class AutocompleteEditView {
 public:
  // Used by the automation system for getting at the model from the view.
  virtual AutocompleteEditModel* model() = 0;
  virtual const AutocompleteEditModel* model() const = 0;

  // For use when switching tabs, this saves the current state onto the tab so
  // that it can be restored during a later call to Update().
  virtual void SaveStateToTab(TabContents* tab) = 0;

  // Called when any LocationBarView state changes. If
  // |tab_for_state_restoring| is non-NULL, it points to a TabContents whose
  // state we should restore.
  virtual void Update(const TabContents* tab_for_state_restoring) = 0;

  // Asks the browser to load the specified URL, which is assumed to be one of
  // the popup entries, using the supplied disposition and transition type.
  // |alternate_nav_url|, if non-empty, contains the alternate navigation URL
  // for |url|.  See comments on AutocompleteResult::GetAlternateNavURL().
  //
  // |selected_line| is passed to SendOpenNotification(); see comments there.
  //
  // If the URL was expanded from a keyword, |keyword| is that keyword.
  //
  // This may close the popup.
  virtual void OpenURL(const GURL& url,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition,
                       const GURL& alternate_nav_url,
                       size_t selected_line,
                       const std::wstring& keyword) = 0;

  // Returns the current text of the edit control, which could be the
  // "temporary" text set by the popup, the "permanent" text set by the
  // browser, or just whatever the user has currently typed.
  virtual std::wstring GetText() const = 0;

  // The user text is the text the user has manually keyed in.  When present,
  // this is shown in preference to the permanent text; hitting escape will
  // revert to the permanent text.
  virtual void SetUserText(const std::wstring& text) = 0;
  virtual void SetUserText(const std::wstring& text,
                           const std::wstring& display_text,
                           bool update_popup) = 0;

  // Sets the window text and the caret position.
  virtual void SetWindowTextAndCaretPos(const std::wstring& text,
                                        size_t caret_pos) = 0;

  // Returns true if all text is selected.
  virtual bool IsSelectAll() = 0;

  // Selects all the text in the edit.  Use this in place of SetSelAll() to
  // avoid selecting the "phantom newline" at the end of the edit.
  virtual void SelectAll(bool reversed) = 0;

  // Reverts the edit and popup back to their unedited state (permanent text
  // showing, popup closed, no user input in progress).
  virtual void RevertAll() = 0;

  // Updates the autocomplete popup and other state after the text has been
  // changed by the user.
  virtual void UpdatePopup() = 0;

  // Closes the autocomplete popup, if it's open.
  virtual void ClosePopup() = 0;

  // Called when the temporary text in the model may have changed.
  // |display_text| is the new text to show; |save_original_selection| is true
  // when there wasn't previously a temporary text and thus we need to save off
  // the user's existing selection.
  virtual void OnTemporaryTextMaybeChanged(const std::wstring& display_text,
                                           bool save_original_selection) = 0;

  // Called when the inline autocomplete text in the model may have changed.
  // |display_text| is the new text to show; |user_text_length| is the length of
  // the user input portion of that (so, up to but not including the inline
  // autocompletion).  Returns whether the display text actually changed.
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const std::wstring& display_text, size_t user_text_length) = 0;

  // Called when the temporary text has been reverted by the user.  This will
  // reset the user's original selection.
  virtual void OnRevertTemporaryText() = 0;

  // Every piece of code that can change the edit should call these functions
  // before and after the change.  These functions determine if anything
  // meaningful changed, and do any necessary updating and notification.
  virtual void OnBeforePossibleChange() = 0;
  // OnAfterPossibleChange() returns true if there was a change that caused it
  // to call UpdatePopup().
  virtual bool OnAfterPossibleChange() = 0;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_H_
