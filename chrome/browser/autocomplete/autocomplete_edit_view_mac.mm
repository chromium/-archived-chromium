// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view_mac.h"

// Thin Obj-C bridge class that is the delegate of the omnibox field.
// It intercepts various control delegate methods and vectors them to
// the edit view.

@interface AutocompleteFieldDelegate : NSObject {
 @private
  AutocompleteEditViewMac* edit_view_;  // weak, owns us.
}
- initWithEditView:(AutocompleteEditViewMac*)view;
@end

AutocompleteEditViewMac::AutocompleteEditViewMac(
    AutocompleteEditController* controller,
    ToolbarModel* toolbar_model,
    Profile* profile,
    CommandUpdater* command_updater)
    : model_(new AutocompleteEditModel(this, controller, profile)),
      popup_view_(new AutocompletePopupViewMac(this, model_.get(), profile)),
      controller_(controller),
      toolbar_model_(toolbar_model),
      command_updater_(command_updater),
      field_(nil),
      edit_helper_([[AutocompleteFieldDelegate alloc] initWithEditView:this]) {
  DCHECK(controller);
  DCHECK(toolbar_model);
  DCHECK(profile);
  DCHECK(command_updater);
}

AutocompleteEditViewMac::~AutocompleteEditViewMac() {
  // TODO(shess): Having to be aware of destructor ordering in this
  // way seems brittle.  There must be a better way.

  // Destroy popup view before this object in case it tries to call us
  // back in the destructor.  Likewise for destroying the model before
  // this object.
  popup_view_.reset();
  model_.reset();

  // Disconnect field_ from edit_helper_ so that we don't get calls
  // after destruction.
  [field_ setDelegate:nil];
}

// TODO(shess): This is the minimal change which seems to unblock
// getting the minimal Omnibox code checked in without making the
// world worse.  Browser::TabSelectedAt() calls this when the tab
// changes, but that is only wired up for Windows.  I do not yet
// understand that code well enough to go for it.  Once wired up, then
// code can be removed at:
// [TabContentsController defocusLocationBar]
// [TabStripController selectTabWithContents:...]
void AutocompleteEditViewMac::SaveStateToTab(TabContents* tab) {
  // TODO(shess): Actually save the state to the tab area.

  // Drop the popup before we change to another tab.
  ClosePopup();
}

void AutocompleteEditViewMac::OpenURL(const GURL& url,
				      WindowOpenDisposition disposition,
				      PageTransition::Type transition,
				      const GURL& alternate_nav_url,
				      size_t selected_line,
				      const std::wstring& keyword) {
  // TODO(shess): Why is the caller passing an invalid url in the
  // first place?  Make sure that case isn't being dropped on the
  // floor.
  if (!url.is_valid()) {
    return;
  }

  model_->SendOpenNotification(selected_line, keyword);

  if (disposition != NEW_BACKGROUND_TAB)
    RevertAll();  // Revert the box to its unedited state.
  controller_->OnAutocompleteAccept(url, disposition, transition,
				    alternate_nav_url);
}

std::wstring AutocompleteEditViewMac::GetText() const {
  return base::SysNSStringToWide([field_ stringValue]);
}

void AutocompleteEditViewMac::SetWindowTextAndCaretPos(const std::wstring& text,
						       size_t caret_pos) {
  UpdateAndStyleText(text, text.size());
}

void AutocompleteEditViewMac::SelectAll(bool reversed) {
  // TODO(shess): Figure out what reversed implies.  The gtk version
  // has it imply inverting the selection front to back, but I don't
  // even know if that makes sense for Mac.
  UpdateAndStyleText(GetText(), 0);
}

void AutocompleteEditViewMac::RevertAll() {
  ClosePopup();
  model_->Revert();

  std::wstring tt = GetText();
  UpdateAndStyleText(tt, tt.size());
  controller_->OnChanged();
}

void AutocompleteEditViewMac::UpdatePopup() {
  model_->SetInputInProgress(true);
  if (!model_->has_focus())
    return;

  // TODO(shess):
  // Shouldn't inline autocomplete when the caret/selection isn't at
  // the end of the text.
  //
  // One option would seem to be to check for a non-nil field
  // editor, and check it's selected range against its length.
  model_->StartAutocomplete(false);
}

void AutocompleteEditViewMac::ClosePopup() {
  popup_view_->StopAutocomplete();
}

void AutocompleteEditViewMac::UpdateAndStyleText(
    const std::wstring& display_text, size_t user_text_length) {
  NSString* ss = base::SysWideToNSString(display_text);
  NSMutableAttributedString* as =
      [[[NSMutableAttributedString alloc] initWithString:ss] autorelease];

  url_parse::Parsed parts;
  AutocompleteInput::Parse(display_text, model_->GetDesiredTLD(),
			   &parts, NULL);
  bool emphasize = model_->CurrentTextIsURL() && (parts.host.len > 0);
  if (emphasize) {
    // TODO(shess): Pull color out as a constant.
    [as addAttribute:NSForegroundColorAttributeName
               value:[NSColor greenColor]
               range:NSMakeRange((NSInteger)parts.host.begin,
                                 (NSInteger)parts.host.end())];
  }

  // TODO(shess): GTK has this as a member var, figure out why.
  ToolbarModel::SecurityLevel scheme_security_level =
      toolbar_model_->GetSchemeSecurityLevel();

  // Emphasize the scheme for security UI display purposes (if necessary).
  if (!model_->user_input_in_progress() && parts.scheme.is_nonempty() &&
      (scheme_security_level != ToolbarModel::NORMAL)) {
    // TODO(shess): Pull colors out as constants.
    NSColor* color;
    if (scheme_security_level == ToolbarModel::SECURE) {
      color = [NSColor blueColor];
    } else {
      color = [NSColor blackColor];
    }
    [as addAttribute:NSForegroundColorAttributeName value:color
               range:NSMakeRange((NSInteger)parts.scheme.begin,
                                 (NSInteger)parts.scheme.end())];
  }

  // TODO(shess): Check that this updates the model's sense of focus
  // correctly.
  [field_ setObjectValue:as];
  if (![field_ currentEditor]) {
    [field_ becomeFirstResponder];
    DCHECK_EQ(field_, [[field_ window] firstResponder]);
  }

  NSRange selected_range = NSMakeRange(user_text_length, [as length]);
  // TODO(shess): What if it didn't get first responder, and there is
  // no field editor?  This will do nothing.  Well, at least it won't
  // crash.  Think of something more productive to do, or prove that
  // it cannot occur and DCHECK appropriately.
  [[field_ currentEditor] setSelectedRange:selected_range];
}

void AutocompleteEditViewMac::OnTemporaryTextMaybeChanged(
    const std::wstring& display_text, bool save_original_selection) {
  // TODO(shess): I believe this is for when the user arrows around
  // the popup, will be restored if they hit escape.  Figure out if
  // that is for certain it.
  if (save_original_selection) {
    saved_temporary_text_ = GetText();
  }

  UpdateAndStyleText(display_text, display_text.size());
}

bool AutocompleteEditViewMac::OnInlineAutocompleteTextMaybeChanged(
    const std::wstring& display_text, size_t user_text_length) {
  // TODO(shess): Make sure that this actually works.  The round trip
  // to native form and back may mean that it's the same but not the
  // same.
  if (display_text == GetText()) {
    return false;
  }

  UpdateAndStyleText(display_text, user_text_length);
  return true;
}

void AutocompleteEditViewMac::OnRevertTemporaryText() {
  UpdateAndStyleText(saved_temporary_text_, saved_temporary_text_.size());
  saved_temporary_text_.clear();
}

void AutocompleteEditViewMac::OnUpOrDownKeyPressed(int dir) {
  model_->OnUpOrDownKeyPressed(dir);
}
void AutocompleteEditViewMac::OnEscapeKeyPressed() {
  model_->OnEscapeKeyPressed();
}
void AutocompleteEditViewMac::OnSetFocus(bool f) {
  model_->OnSetFocus(f);
}
void AutocompleteEditViewMac::OnKillFocus() {
  model_->OnKillFocus();
}
void AutocompleteEditViewMac::AcceptInput(
    WindowOpenDisposition disposition, bool for_drop) {
  model_->AcceptInput(disposition, for_drop);
}
void AutocompleteEditViewMac::OnAfterPossibleChange(
    const std::wstring& new_text,
    bool selection_differs,
    bool text_differs,
    bool just_deleted_text,
    bool at_end_of_edit) {
  model_->OnAfterPossibleChange(new_text, selection_differs, text_differs,
				just_deleted_text, at_end_of_edit);
}
void AutocompleteEditViewMac::SetField(NSTextField* field) {
  field_ = field;
  [field_ setDelegate:edit_helper_];

  // The popup code needs the field for sizing and placement.
  popup_view_->SetField(field_);
}

void AutocompleteEditViewMac::FocusLocation() {
  [[field_ window] makeFirstResponder:field_];
}

@implementation AutocompleteFieldDelegate

- initWithEditView:(AutocompleteEditViewMac*)view {
  self = [super init];
  if (self) {
    edit_view_ = view;
  }
  return self;
}

- (BOOL)control:(NSControl*)control
       textView:(NSTextView*)textView doCommandBySelector:(SEL)cmd {
  if (cmd == @selector(moveDown:)) {
    edit_view_->OnUpOrDownKeyPressed(1);
    return YES;
  }
  
  if (cmd == @selector(moveUp:)) {
    edit_view_->OnUpOrDownKeyPressed(-1);
    return YES;
  }

  if (cmd == @selector(cancelOperation:)) {
    edit_view_->OnEscapeKeyPressed();
    return YES;
  }

  if (cmd == @selector(insertNewline:)) {
    edit_view_->AcceptInput(CURRENT_TAB, false);
    return YES;
  }
  
  return NO;
}

- (void)controlTextDidBeginEditing:(NSNotification*)aNotification {
  edit_view_->OnSetFocus(false);
}

- (void)controlTextDidChange:(NSNotification*)aNotification {
  // TODO(shess): Make this more efficient?  Or not.  For now, just
  // pass in the current text, indicating that the text and
  // selection differ, ignoring deletions, and assuming that we're
  // at the end of the text.
  edit_view_->OnAfterPossibleChange(edit_view_->GetText(),
				    true, true, false, true);
}

- (void)controlTextDidEndEditing:(NSNotification*)aNotification {
  edit_view_->OnKillFocus();

  // TODO(shess): Figure out where the selection belongs.  On GTK,
  // it's set to the start of the text.
}

@end
