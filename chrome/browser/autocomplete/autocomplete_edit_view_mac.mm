// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view_mac.h"
#include "chrome/browser/tab_contents/tab_contents.h"

namespace {

// TODO(shess): This is ugly, find a better way.  Using it right now
// so that I can crib from gtk and still be able to see that I'm using
// the same values easily.
const NSColor* ColorWithRGBBytes(int rr, int gg, int bb) {
  DCHECK_LE(rr, 255);
  DCHECK_LE(bb, 255);
  DCHECK_LE(gg, 255);
  return [NSColor colorWithCalibratedRed:static_cast<float>(rr)/255.0
                                   green:static_cast<float>(gg)/255.0
                                    blue:static_cast<float>(bb)/255.0
                                   alpha:1.0];
}
const NSColor* SecureBackgroundColor() {
  return ColorWithRGBBytes(255, 245, 195);  // Yellow
}
const NSColor* NormalBackgroundColor() {
  return [NSColor controlBackgroundColor];
}
const NSColor* InsecureBackgroundColor() {
  return [NSColor controlBackgroundColor];
}

const NSColor* HostTextColor() {
  return [NSColor blackColor];
}
const NSColor* BaseTextColor() {
  return [NSColor darkGrayColor];
}
const NSColor* SecureSchemeColor() {
  return ColorWithRGBBytes(0x00, 0x96, 0x14);
}
const NSColor* InsecureSchemeColor() {
  return ColorWithRGBBytes(0xc8, 0x00, 0x00);
}

// Store's the model and view state across tab switches.
struct AutocompleteEditViewMacState {
  AutocompleteEditViewMacState(const AutocompleteEditModel::State model_state,
                               const bool has_focus, const NSRange& selection)
      : model_state(model_state),
        has_focus(has_focus),
        selection(selection) {
  }

  const AutocompleteEditModel::State model_state;
  const bool has_focus;
  const NSRange selection;
};

// Returns a lazily initialized property bag accessor for saving our
// state in a TabContents.  When constructed |accessor| generates a
// globally-unique id used to index into the per-tab PropertyBag used
// to store the state data.
PropertyAccessor<AutocompleteEditViewMacState>* GetStateAccessor() {
  static PropertyAccessor<AutocompleteEditViewMacState> accessor;
  return &accessor;
}

// Accessors for storing and getting the state from the tab.
void StoreStateToTab(TabContents* tab,
                     const AutocompleteEditViewMacState& state) {
  GetStateAccessor()->SetProperty(tab->property_bag(), state);
}
const AutocompleteEditViewMacState* GetStateFromTab(const TabContents* tab) {
  return GetStateAccessor()->GetProperty(tab->property_bag());
}

// Helper to make converting url_parse ranges to NSRange easier to
// read.
NSRange ComponentToNSRange(const url_parse::Component& component) {
  return NSMakeRange(static_cast<NSInteger>(component.begin),
                     static_cast<NSInteger>(component.len));
}

}  // namespace

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
    CommandUpdater* command_updater,
    NSTextField* field)
    : model_(new AutocompleteEditModel(this, controller, profile)),
      popup_view_(new AutocompletePopupViewMac(this, model_.get(), profile,
                                               field)),
      controller_(controller),
      toolbar_model_(toolbar_model),
      command_updater_(command_updater),
      field_(field),
      edit_helper_([[AutocompleteFieldDelegate alloc] initWithEditView:this]) {
  DCHECK(controller);
  DCHECK(toolbar_model);
  DCHECK(profile);
  DCHECK(command_updater);
  DCHECK(field);
  [field_ setDelegate:edit_helper_];

  // Needed so that editing doesn't lose the styling.
  [field_ setAllowsEditingTextAttributes:YES];
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

void AutocompleteEditViewMac::SaveStateToTab(TabContents* tab) {
  DCHECK(tab);

  NSRange range;
  if (model_->has_focus()) {
    range = GetSelectedRange();
  } else {
    // If we are not focussed, there is no selection.  Manufacture
    // something reasonable in case it starts to matter in the future.
    range = NSMakeRange(0, [[field_ stringValue] length]);
  }

  AutocompleteEditViewMacState state(model_->GetStateForTabSwitch(),
                                     model_->has_focus(), range);
  StoreStateToTab(tab, state);
}

void AutocompleteEditViewMac::Update(
    const TabContents* tab_for_state_restoring) {
  // TODO(shess): It seems like if the tab is non-NULL, then this code
  // shouldn't need to be called at all.  When coded that way, I find
  // that the field isn't always updated correctly.  Figure out why
  // this is.  Maybe this method should be refactored into more
  // specific cases.
  const std::wstring text = toolbar_model_->GetText();
  const bool user_visible = model_->UpdatePermanentText(text);

  if (tab_for_state_restoring) {
    RevertAll();

    const AutocompleteEditViewMacState* state = 
        GetStateFromTab(tab_for_state_restoring);
    if (state) {
      // Should restore the user's text via SetUserText().
      model_->RestoreState(state->model_state);

      // Restore user's selection.
      // TODO(shess): The model_ does not restore the focus state.  If
      // field_ was in focus when we switched away, I presume it
      // should be in focus when we switch back.  Figure out if model_
      // not restoring focus is an oversight, or intentional for some
      // subtle reason.
      if (state->has_focus) {
        SetSelectedRange(state->selection);
      }
    }
  } else if (user_visible) {
    // Restore everything to the baseline look.
    RevertAll();
    // TODO(shess): Figure out how this case is used, to make sure
    // we're getting the selection and popup right.

  } else {
    // TODO(shess): Figure out how this case is used, to make sure
    // we're getting the selection and popup right.
    UpdateAndStyleText(text);
  }
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

void AutocompleteEditViewMac::SetUserText(const std::wstring& text,
                                          const std::wstring& display_text,
                                          bool update_popup) {
  model_->SetUserText(text);
  // TODO(shess): TODO below from gtk.
  // TODO(deanm): something about selection / focus change here.
  UpdateAndStyleText(display_text);
  if (update_popup) {
    UpdatePopup();
  }
}

NSRange AutocompleteEditViewMac::GetSelectedRange() const {
  DCHECK([field_ currentEditor]);
  return [[field_ currentEditor] selectedRange];
}

void AutocompleteEditViewMac::SetSelectedRange(const NSRange range) {
  // TODO(shess): Check if we should steal focus or not.  We can't set
  // the selection without focus, though.
  FocusLocation();

  // TODO(shess): What if it didn't get first responder, and there is
  // no field editor?  This will do nothing.  Well, at least it won't
  // crash.  Think of something more productive to do, or prove that
  // it cannot occur and DCHECK appropriately.
  [[field_ currentEditor] setSelectedRange:range];
}

void AutocompleteEditViewMac::SetWindowTextAndCaretPos(const std::wstring& text,
                                                       size_t caret_pos) {
  DCHECK_LE(caret_pos, text.size());
  UpdateAndStyleText(text);
  SetSelectedRange(NSMakeRange(caret_pos, caret_pos));
}

void AutocompleteEditViewMac::SelectAll(bool reversed) {
  // TODO(shess): Figure out what |reversed| implies.  The gtk version
  // has it imply inverting the selection front to back, but I don't
  // even know if that makes sense for Mac.

  // TODO(shess): Verify that we should be stealing focus at this
  // point.
  SetSelectedRange(NSMakeRange(0, GetText().size()));
}

void AutocompleteEditViewMac::RevertAll() {
  ClosePopup();
  model_->Revert();

  UpdateAndStyleText(GetText());
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
  popup_view_->GetModel()->StopAutocomplete();
}

void AutocompleteEditViewMac::UpdateAndStyleText(
    const std::wstring& display_text) {
  NSString* ss = base::SysWideToNSString(display_text);
  NSMutableAttributedString* as =
      [[[NSMutableAttributedString alloc] initWithString:ss] autorelease];
  [as addAttribute:NSFontAttributeName value:[field_ font]
             range:NSMakeRange(0, [as length])];

  url_parse::Parsed parts;
  AutocompleteInput::Parse(display_text, model_->GetDesiredTLD(),
                           &parts, NULL);
  const bool emphasize = model_->CurrentTextIsURL() && (parts.host.len > 0);
  if (emphasize) {
    [as addAttribute:NSForegroundColorAttributeName value:BaseTextColor()
               range:NSMakeRange(0, [as length])];

    [as addAttribute:NSForegroundColorAttributeName value:HostTextColor()
               range:ComponentToNSRange(parts.host)];
  }

  // TODO(shess): GTK has this as a member var, figure out why.
  // [Could it be to not change if no change?  If so, I'm guessing
  // AppKit may already handle that.]
  const ToolbarModel::SecurityLevel scheme_security_level =
      toolbar_model_->GetSchemeSecurityLevel();

  if (scheme_security_level == ToolbarModel::SECURE) {
    [field_ setBackgroundColor:SecureBackgroundColor()];
  } else if (scheme_security_level == ToolbarModel::NORMAL) {
    [field_ setBackgroundColor:NormalBackgroundColor()];
  } else if (scheme_security_level == ToolbarModel::INSECURE) {
    [field_ setBackgroundColor:InsecureBackgroundColor()];
  } else {
    NOTREACHED() << "Unexpected scheme_security_level: "
                 << scheme_security_level;
  }

  // Emphasize the scheme for security UI display purposes (if necessary).
  if (!model_->user_input_in_progress() && parts.scheme.is_nonempty() &&
      (scheme_security_level != ToolbarModel::NORMAL)) {
    NSColor* color;
    if (scheme_security_level == ToolbarModel::SECURE) {
      color = SecureSchemeColor();
    } else {
      color = InsecureSchemeColor();
    }
    [as addAttribute:NSForegroundColorAttributeName value:color
               range:ComponentToNSRange(parts.scheme)];
  }

  [field_ setObjectValue:as];
}

void AutocompleteEditViewMac::OnTemporaryTextMaybeChanged(
    const std::wstring& display_text, bool save_original_selection) {
  // TODO(shess): I believe this is for when the user arrows around
  // the popup, will be restored if they hit escape.  Figure out if
  // that is for certain it.
  if (save_original_selection) {
    saved_temporary_selection_ = GetSelectedRange();
    saved_temporary_text_ = GetText();
  }

  SetWindowTextAndCaretPos(display_text, display_text.size());
}

bool AutocompleteEditViewMac::OnInlineAutocompleteTextMaybeChanged(
    const std::wstring& display_text, size_t user_text_length) {
  // TODO(shess): Make sure that this actually works.  The round trip
  // to native form and back may mean that it's the same but not the
  // same.
  if (display_text == GetText()) {
    return false;
  }

  UpdateAndStyleText(display_text);
  DCHECK_LE(user_text_length, display_text.size());
  SetSelectedRange(NSMakeRange(user_text_length, display_text.size()));
  return true;
}

void AutocompleteEditViewMac::OnRevertTemporaryText() {
  UpdateAndStyleText(saved_temporary_text_);
  saved_temporary_text_.clear();
  SetSelectedRange(saved_temporary_selection_);
}

void AutocompleteEditViewMac::OnBeforePossibleChange() {
  selection_before_change_ = GetSelectedRange();
  text_before_change_ = GetText();
}

bool AutocompleteEditViewMac::OnAfterPossibleChange() {
  const NSRange new_selection(GetSelectedRange());
  const std::wstring new_text(GetText());
  const size_t length = new_text.length();

  const bool selection_differs = !NSEqualRanges(new_selection,
                                                selection_before_change_);
  const bool at_end_of_edit = (length == new_selection.location);
  const bool text_differs = (new_text != text_before_change_);

  // When the user has deleted text, we don't allow inline
  // autocomplete.  This is assumed if the text has gotten shorter AND
  // the selection has shifted towards the front of the text.  During
  // normal typing the text will almost always be shorter (as the new
  // input replaces the autocomplete suggestion), but in that case the
  // selection point will have moved towards the end of the text.
  // TODO(shess): In our implementation, we can catch -deleteBackward:
  // and other methods to provide positive knowledge that a delete
  // occured, rather than intuiting it from context.  Consider whether
  // that would be a stronger approach.
  const bool just_deleted_text =
      (length < text_before_change_.length() &&
       new_selection.location <= selection_before_change_.location);

  const bool something_changed = model_->OnAfterPossibleChange(new_text,
      selection_differs, text_differs, just_deleted_text, at_end_of_edit);

  // Restyle if the user changed something.
  // TODO(shess): Does this need to diver deeper to avoid flashing?
  // For instance, if the user types "/" after the hostname, will it
  // show as HostTextColor() for an instant before being replaced by
  // BaseTextColor().  This could probably be done by subclassing the
  // cell and intercepting draw requests so that we draw the right
  // thing every time.
  // NOTE(shess): That kind of thing would also help with when the
  // flashing from when the user's typing replaces the selection which
  // is then re-created with the new autocomplete results.
  if (something_changed) {
    UpdateAndStyleText(new_text);
    SetSelectedRange(new_selection);
  }

  return something_changed;
}

void AutocompleteEditViewMac::OnUpOrDownKeyPressed(bool up, bool by_page) {
  const int count = by_page ? model_->result().size() : 1;
  model_->OnUpOrDownKeyPressed(up ? -count : count);
}
void AutocompleteEditViewMac::OnEscapeKeyPressed() {
  model_->OnEscapeKeyPressed();
}
void AutocompleteEditViewMac::OnSetFocus(bool f) {
  model_->OnSetFocus(f);
}
void AutocompleteEditViewMac::OnKillFocus() {
  // TODO(shess): This would seem to be a job for |model_|.
  ClosePopup();

  // Tell the model to reset itself.
  model_->OnKillFocus();
}
void AutocompleteEditViewMac::AcceptInput(
    WindowOpenDisposition disposition, bool for_drop) {
  model_->AcceptInput(disposition, for_drop);
}

void AutocompleteEditViewMac::FocusLocation() {
  // -makeFirstResponder: will select the entire field_.  If we're
  // already firstResponder, it's likely that we want to retain the
  // current selection.
  if (![field_ currentEditor]) {
    [[field_ window] makeFirstResponder:field_];
    DCHECK_EQ([field_ currentEditor], [[field_ window] firstResponder]);
  }
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
    edit_view_->OnUpOrDownKeyPressed(false, false);
    return YES;
  }
  
  if (cmd == @selector(moveUp:)) {
    edit_view_->OnUpOrDownKeyPressed(true, false);
    return YES;
  }
  
  if (cmd == @selector(scrollPageDown:)) {
    edit_view_->OnUpOrDownKeyPressed(false, true);
    return YES;
  }
  
  if (cmd == @selector(scrollPageUp:)) {
    edit_view_->OnUpOrDownKeyPressed(true, true);
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

  // Capture the state before the operation changes the content.
  // TODO(shess): Determine if this is always redundent WRT the call
  // in -controlTextDidChange:.
  edit_view_->OnBeforePossibleChange();
  return NO;
}

- (void)controlTextDidBeginEditing:(NSNotification*)aNotification {
  edit_view_->OnSetFocus(false);

  // Capture the current state.
  edit_view_->OnBeforePossibleChange();
}

- (void)controlTextDidChange:(NSNotification*)aNotification {
  // Figure out what changed and notify the model_.
  edit_view_->OnAfterPossibleChange();

  // Then capture the new state.
  edit_view_->OnBeforePossibleChange();
}

- (BOOL)control:(NSControl*)control textShouldEndEditing:(NSText*)fieldEditor {
  edit_view_->OnKillFocus();

  return YES;

  // TODO(shess): Figure out where the selection belongs.  On GTK,
  // it's set to the start of the text.
}

@end
