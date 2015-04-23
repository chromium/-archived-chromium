// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_MAC_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompleteEditController;
@class AutocompleteEditHelper;
class AutocompleteEditModel;
class AutocompletePopupViewMac;
class CommandUpdater;
class Profile;
class TabContents;
class ToolbarModel;

// Implements AutocompleteEditView on an NSTextField.

class AutocompleteEditViewMac : public AutocompleteEditView {
 public:
  AutocompleteEditViewMac(AutocompleteEditController* controller,
                          ToolbarModel* toolbar_model,
                          Profile* profile,
                          CommandUpdater* command_updater,
                          NSTextField* field);
  virtual ~AutocompleteEditViewMac();

  // Implement the AutocompleteEditView interface.
  // TODO(shess): See if this couldn't be simplified to:
  //    virtual AEM* model() const { ... }
  virtual AutocompleteEditModel* model() { return model_.get(); }
  virtual const AutocompleteEditModel* model() const { return model_.get(); }

  virtual void SaveStateToTab(TabContents* tab);
  virtual void Update(const TabContents* tab_for_state_restoring);

  virtual void OpenURL(const GURL& url,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition,
                       const GURL& alternate_nav_url,
                       size_t selected_line,
                       const std::wstring& keyword);

  virtual std::wstring GetText() const;
  virtual void SetUserText(const std::wstring& text) {
    SetUserText(text, text, true);
  }
  virtual void SetUserText(const std::wstring& text,
                           const std::wstring& display_text,
                           bool update_popup);

  virtual void SetWindowTextAndCaretPos(const std::wstring& text,
                                        size_t caret_pos);

  virtual void SetForcedQuery() { NOTIMPLEMENTED(); }

  virtual bool IsSelectAll() {
    NOTIMPLEMENTED();
    return false;
  }

  virtual void SelectAll(bool reversed);
  virtual void RevertAll();
  virtual void UpdatePopup();
  virtual void ClosePopup();
  virtual void OnTemporaryTextMaybeChanged(const std::wstring& display_text,
                                           bool save_original_selection);
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const std::wstring& display_text, size_t user_text_length);
  virtual void OnRevertTemporaryText();
  virtual void OnBeforePossibleChange();
  virtual bool OnAfterPossibleChange();

  // Helper functions for use from AutocompleteEditHelper Objective-C
  // class.
  void OnUpOrDownKeyPressed(bool up, bool by_page);
  void OnEscapeKeyPressed();

  // Called when editing begins in the field, and before the results
  // of any editing are communicated to |model_|.
  void OnWillBeginEditing();

  // Called when editing ends in the field.
  void OnDidEndEditing();

  // Called when the window |field_| is in loses key to clean up
  // visual state (such as closing the popup).
  void OnDidResignKey();

  void AcceptInput(WindowOpenDisposition disposition, bool for_drop);

  // Helper for LocationBarBridge.
  void FocusLocation();

 private:
  // Returns the field's currently selected range.  Only valid if the
  // field has focus.
  NSRange GetSelectedRange() const;

  // Returns true if |field_| is first-responder in the window.  Used
  // in various DCHECKS to make sure code is running in appropriate
  // situations.
  bool IsFirstResponder() const;

  // If |model_| believes it has focus, grab focus if needed and set
  // the selection to |range|.  Otherwise does nothing.
  void SetSelectedRange(const NSRange range);

  // Update the field with |display_text| and highlight the host and
  // scheme (if it's an URL or URL-fragment).
  void SetText(const std::wstring& display_text);

  // Update the field with |display_text| and set the selection.
  void SetTextAndSelectedRange(const std::wstring& display_text,
                               const NSRange range);

  // Pass the current content of |field_| to SetText(), maintaining
  // any selection.  Named to be consistent with GTK and Windows,
  // though here we cannot really do the in-place operation they do.
  void EmphasizeURLComponents();

  scoped_ptr<AutocompleteEditModel> model_;
  scoped_ptr<AutocompletePopupViewMac> popup_view_;

  AutocompleteEditController* controller_;
  ToolbarModel* toolbar_model_;

  // The object that handles additional command functionality exposed on the
  // edit, such as invoking the keyword editor.
  CommandUpdater* command_updater_;

  NSTextField* field_;  // owned by tab controller

  // Objective-C object to bridge field_ delegate calls to C++.
  scoped_nsobject<AutocompleteEditHelper> edit_helper_;

  // Text and selection at the point where the user started using the
  // arrows to move around in the popup.
  NSRange saved_temporary_selection_;
  std::wstring saved_temporary_text_;

  // Tracking state before and after a possible change for reporting
  // to model_.
  NSRange selection_before_change_;
  std::wstring text_before_change_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteEditViewMac);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_MAC_H_
