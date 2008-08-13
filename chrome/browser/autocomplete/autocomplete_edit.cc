// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/autocomplete/autocomplete_edit.h"

#include <locale>

#include "base/base_drag_source.h"
#include "base/clipboard_util.h"
#include "base/gfx/skia_utils.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/autocomplete/edit_drop_target.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/controller.h"
#include "chrome/browser/drag_utils.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/template_url.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/browser/url_fixer_upper.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/common/clipboard_service.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/utils.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/win_util.h"
#include "chrome/views/accessibility/autocomplete_accessibility.h"
#include "googleurl/src/url_util.h"

#include "generated_resources.h"

#pragma comment(lib, "oleacc.lib")  // Needed for accessibility support.

// TODO (jcampan): these colors should be derived from the system colors to
// ensure they show properly. Bug #948807.
// Colors used to emphasize the scheme in the URL.
static const COLORREF kSecureSchemeColor = RGB(0, 150, 20);
static const COLORREF kInsecureSchemeColor = RGB(200, 0, 0);

// Colors used to strike-out the scheme when it is insecure.
static const SkColor kSchemeStrikeoutColor = SkColorSetRGB(210, 0, 0);
static const SkColor kSchemeSelectedStrikeoutColor =
    SkColorSetRGB(255, 255, 255);

///////////////////////////////////////////////////////////////////////////////
// Helper classes

AutocompleteEdit::ScopedFreeze::ScopedFreeze(AutocompleteEdit* edit,
                                             ITextDocument* text_object_model)
    : edit_(edit),
      text_object_model_(text_object_model) {
  // Freeze the screen.
  if (text_object_model_) {
    long count;
    text_object_model_->Freeze(&count);
  }
}

AutocompleteEdit::ScopedFreeze::~ScopedFreeze() {
  // Unfreeze the screen.
  // NOTE: If this destructor is reached while the edit is being destroyed (for
  // example, because we double-clicked the edit of a popup and caused it to
  // transform to an unconstrained window), it will no longer have an HWND, and
  // text_object_model_ may point to a destroyed object, so do nothing here.
  if (edit_->IsWindow() && text_object_model_) {
    long count;
    text_object_model_->Unfreeze(&count);
    if (count == 0) {
      // We need to UpdateWindow() here instead of InvalidateRect() because, as
      // far as I can tell, the edit likes to synchronously erase its background
      // when unfreezing, thus requiring us to synchronously redraw if we don't
      // want flicker.
      edit_->UpdateWindow();
    }
  }
}

AutocompleteEdit::ScopedSuspendUndo::ScopedSuspendUndo(
    ITextDocument* text_object_model)
    : text_object_model_(text_object_model) {
  // Suspend Undo processing.
  if (text_object_model_)
    text_object_model_->Undo(tomSuspend, NULL);
}

AutocompleteEdit::ScopedSuspendUndo::~ScopedSuspendUndo() {
  // Resume Undo processing.
  if (text_object_model_)
    text_object_model_->Undo(tomResume, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// AutocompleteEdit

// These are used to hook the CRichEditCtrl's calls to BeginPaint() and
// EndPaint() and provide a memory DC instead.  See OnPaint().
static HWND edit_hwnd = NULL;
static PAINTSTRUCT paint_struct;

// A single AutocompleteController used solely for making synchronous calls to
// determine how to deal with the clipboard contents for Paste And Go
// functionality.  We avoid using the popup's controller here because we don't
// want to interrupt in-progress queries or modify the popup state just
// because the user right-clicked the edit.  We don't need a controller for
// every edit because this will always be accessed on the main thread, so we
// won't have thread-safety problems.
static AutocompleteController* paste_and_go_controller = NULL;
static int paste_and_go_controller_refcount = 0;

AutocompleteEdit::AutocompleteEdit(const ChromeFont& font,
                                   Controller* controller,
                                   ToolbarModel* model,
                                   ChromeViews::View* parent_view,
                                   HWND hwnd,
                                   Profile* profile,
                                   CommandController* command_controller,
                                   bool popup_window_mode)
    : controller_(controller),
      model_(model),
      popup_(new AutocompletePopup(font, this, profile)),
      popup_window_mode_(popup_window_mode),
      has_focus_(false),
      user_input_in_progress_(false),
      just_deleted_text_(false),
      has_temporary_text_(false),
      paste_state_(NONE),
      tracking_click_(false),
      tracking_double_click_(false),
      double_click_time_(0),
      can_discard_mousemove_(false),
      control_key_state_(UP),
      command_controller_(command_controller),
      parent_view_(parent_view),
      font_(font),
      profile_(profile),
      possible_drag_(false),
      in_drag_(false),
      initiated_drag_(false),
      drop_highlight_position_(-1),
      is_keyword_hint_(false),
      disable_keyword_ui_(false),
      show_search_hint_(true),
      background_color_(0),
      scheme_security_level_(ToolbarModel::NORMAL) {
  if (!popup_window_mode_ && ++paste_and_go_controller_refcount == 1) {
    // We don't have a controller yet, so create one.  No listener is needed
    // since we'll only be doing synchronous calls, and no profile is set since
    // we'll set this before each call to the controller.
    paste_and_go_controller = new AutocompleteController(NULL, NULL);
  }

  saved_selection_for_focus_change_.cpMin = -1;

  // Statics used for global patching of riched20.dll.
  static HMODULE richedit_module = NULL;
  static iat_patch::IATPatchFunction patch_begin_paint;
  static iat_patch::IATPatchFunction patch_end_paint;

  if (!richedit_module) {
    richedit_module = LoadLibrary(L"riched20.dll");
    if (richedit_module) {
      DCHECK(!patch_begin_paint.is_patched());
      patch_begin_paint.Patch(richedit_module, "user32.dll", "BeginPaint",
                              &BeginPaintIntercept);
      DCHECK(!patch_end_paint.is_patched());
      patch_end_paint.Patch(richedit_module, "user32.dll", "EndPaint",
                            &EndPaintIntercept);
    }
  }

  Create(hwnd, 0, 0, 0, l10n_util::GetExtendedStyles());
  SetReadOnly(popup_window_mode_);
  SetFont(font_.hfont());

  // NOTE: Do not use SetWordBreakProcEx() here, that is no longer supported as
  // of Rich Edit 2.0 onward.
  SendMessage(m_hWnd, EM_SETWORDBREAKPROC, 0,
              reinterpret_cast<LPARAM>(&WordBreakProc));

  // Get the metrics for the font.
  HDC dc = ::GetDC(NULL);
  SelectObject(dc, font_.hfont());
  TEXTMETRIC tm = {0};
  GetTextMetrics(dc, &tm);
  font_ascent_ = tm.tmAscent;
  const float kXHeightRatio = 0.7f;  // The ratio of a font's x-height to its
                                     // cap height.  Sadly, Windows doesn't
                                     // provide a true value for a font's
                                     // x-height in its text metrics, so we
                                     // approximate.
  font_x_height_ = static_cast<int>((static_cast<float>(font_ascent_ -
      tm.tmInternalLeading) * kXHeightRatio) + 0.5);
  // The distance from the top of the field to the desired baseline of the
  // rendered text.
  const int kTextBaseline = popup_window_mode_ ? 15 : 18;
  font_y_adjustment_ = kTextBaseline - font_ascent_;
  font_descent_ = tm.tmDescent;

  // Get the number of twips per pixel, which we need below to offset our text
  // by the desired number of pixels.
  const long kTwipsPerPixel = kTwipsPerInch / GetDeviceCaps(dc, LOGPIXELSY);
  ::ReleaseDC(NULL, dc);

  // Set the default character style -- adjust to our desired baseline and make
  // text grey.
  CHARFORMAT cf = {0};
  cf.dwMask = CFM_OFFSET | CFM_COLOR;
  cf.yOffset = -font_y_adjustment_ * kTwipsPerPixel;
  cf.crTextColor = GetSysColor(COLOR_GRAYTEXT);
  SetDefaultCharFormat(cf);

  // Set up context menu.
  context_menu_.reset(new Menu(this, Menu::TOPLEFT, m_hWnd));
  if (popup_window_mode_) {
    context_menu_->AppendMenuItemWithLabel(IDS_COPY,
                                           l10n_util::GetString(IDS_COPY));
  } else {
    context_menu_->AppendMenuItemWithLabel(IDS_UNDO,
                                           l10n_util::GetString(IDS_UNDO));
    context_menu_->AppendSeparator();
    context_menu_->AppendMenuItemWithLabel(IDS_CUT,
                                           l10n_util::GetString(IDS_CUT));
    context_menu_->AppendMenuItemWithLabel(IDS_COPY,
                                           l10n_util::GetString(IDS_COPY));
    context_menu_->AppendMenuItemWithLabel(IDS_PASTE,
                                           l10n_util::GetString(IDS_PASTE));
    // GetContextualLabel() will override this next label with the
    // IDS_PASTE_AND_SEARCH label as needed.
    context_menu_->AppendMenuItemWithLabel(
        IDS_PASTE_AND_GO, l10n_util::GetString(IDS_PASTE_AND_GO));
    context_menu_->AppendSeparator();
    context_menu_->AppendMenuItemWithLabel(IDS_SELECTALL,
                                           l10n_util::GetString(IDS_SELECTALL));
    context_menu_->AppendSeparator();
    context_menu_->AppendMenuItemWithLabel(
        IDS_EDIT_SEARCH_ENGINES, l10n_util::GetString(IDS_EDIT_SEARCH_ENGINES));
  }

  // By default RichEdit has a drop target. Revoke it so that we can install our
  // own. Revoke takes care of deleting the existing one.
  RevokeDragDrop(m_hWnd);

  // Register our drop target. RichEdit appears to invoke RevokeDropTarget when
  // done so that we don't have to explicitly.
  if (!popup_window_mode_) {
    scoped_refptr<EditDropTarget> drop_target = new EditDropTarget(this);
    RegisterDragDrop(m_hWnd, drop_target.get());
  }
}

AutocompleteEdit::~AutocompleteEdit() {
  NotificationService::current()->Notify(NOTIFY_AUTOCOMPLETE_EDIT_DESTROYED,
      Source<AutocompleteEdit>(this), NotificationService::NoDetails());

  if (!popup_window_mode_ && --paste_and_go_controller_refcount == 0)
    delete paste_and_go_controller;
}

void AutocompleteEdit::Update(const TabContents* tab_for_state_restoring) {
  // When there's a new URL, and the user is not editing anything or the edit
  // doesn't have focus, we want to revert the edit to show the new URL.  (The
  // common case where the edit doesn't have focus is when the user has started
  // an edit and then abandoned it and clicked a link on the page.)
  std::wstring permanent_text = model_->GetText();
  const bool visibly_changed_permanent_text =
      (permanent_text_ != permanent_text) &&
      (!user_input_in_progress_ || !has_focus_);

  permanent_text_ = permanent_text;

  COLORREF background_color =
    LocationBarView::kBackgroundColorByLevel[model_->GetSchemeSecurityLevel()];

  // Bail early when no visible state will actually change (prevents an
  // unnecessary ScopedFreeze, and thus UpdateWindow()).
  if ((background_color == background_color_) &&
      (model_->GetSchemeSecurityLevel() == scheme_security_level_) &&
      !visibly_changed_permanent_text &&
      !tab_for_state_restoring)
    return;

  // Update our local state as desired.  We set scheme_security_level_ here so
  // it will already be correct before we get to any RevertAll()s below and use
  // it.
  ScopedFreeze freeze(this, GetTextObjectModel());
  if (background_color_ != background_color) {
    background_color_ = background_color;
    SetBackgroundColor(background_color_);
  }
  const bool changed_security_level =
      (model_->GetSchemeSecurityLevel() != scheme_security_level_);
  scheme_security_level_ = model_->GetSchemeSecurityLevel();

  // When we're switching to a new tab, restore its state, if any.
  if (tab_for_state_restoring) {
    // Make sure we reset our own state first.  The new tab may not have any
    // saved state, or it may not have had input in progress, in which case we
    // won't overwrite all our local state.
    RevertAll();

    const AutocompleteEdit::State* const state =
        tab_for_state_restoring->saved_location_bar_state();
    if (state) {
      // Restore any user editing.
      if (state->user_input_in_progress) {
        // NOTE: Be sure and set keyword-related state BEFORE invoking
        // DisplayTextFromUserText(), as its result depends upon this state.
        keyword_ = state->keyword;
        is_keyword_hint_ = state->is_keyword_hint;
        disable_keyword_ui_ = state->disable_keyword_ui;
        show_search_hint_ = state->show_search_hint;
        SetUserText(state->user_text, DisplayTextFromUserText(state->user_text),
                    false);
        popup_->manually_selected_match_ = state->manually_selected_match;
      }

      // Restore user's selection.  We do this after restoring the user_text
      // above so we're selecting in the correct string.
      SetSelectionRange(state->selection);
      saved_selection_for_focus_change_ =
          state->saved_selection_for_focus_change;
    }
  } else if (visibly_changed_permanent_text) {
    // Not switching tabs, just updating the permanent text.  (In the case where
    // we _were_ switching tabs, the RevertAll() above already drew the new
    // permanent text.)

    // Tweak: if the edit was previously nonempty and had all the text selected,
    // select all the new text.  This makes one particular case better: the
    // user clicks in the box to change it right before the permanent URL is
    // changed.  Since the new URL is still fully selected, the user's typing
    // will replace the edit contents as they'd intended.
    //
    // NOTE: The selection can be longer than the text length if the edit is in
    // in rich text mode and the user has selected the "phantom newline" at the
    // end, so use ">=" instead of "==" to see if all the text is selected.  In
    // theory we prevent this case from ever occurring, but this is still safe.
    CHARRANGE sel;
    GetSelection(sel);
    const bool was_reversed = (sel.cpMin > sel.cpMax);
    const bool was_sel_all = (sel.cpMin != sel.cpMax) && IsSelectAll(sel);

    RevertAll();

    if (was_sel_all)
      SelectAll(was_reversed);
  } else if (changed_security_level) {
    // Only the security style changed, nothing else.  Redraw our text using it.
    EmphasizeURLComponents();
  }
}

void AutocompleteEdit::SetProfile(Profile* profile) {
  DCHECK(profile);
  profile_ = profile;
  popup_->SetProfile(profile);
}

void AutocompleteEdit::SaveStateToTab(TabContents* tab) {
  DCHECK(tab);

  // Like typing, switching tabs "accepts" the temporary text as the user
  // text, because it makes little sense to have temporary text when the
  // popup is closed.
  if (user_input_in_progress_)
    InternalSetUserText(UserTextFromDisplayText(GetText()));

  CHARRANGE selection;
  GetSelection(selection);
  tab->set_saved_location_bar_state(new State(selection,
      saved_selection_for_focus_change_, user_input_in_progress_, user_text_,
      popup_->manually_selected_match_, keyword_, is_keyword_hint_,
      disable_keyword_ui_, show_search_hint_));
}

std::wstring AutocompleteEdit::GetText() const {
  const int len = GetTextLength() + 1;
  std::wstring str;
  GetWindowText(WriteInto(&str, len), len);
  return str;
}

std::wstring AutocompleteEdit::GetURLForCurrentText(
    PageTransition::Type* transition,
    bool* is_history_what_you_typed_match,
    std::wstring* alternate_nav_url) {
  return (popup_->is_open() || popup_->query_in_progress()) ?
      popup_->URLsForCurrentSelection(transition,
                                      is_history_what_you_typed_match,
                                      alternate_nav_url) :
      popup_->URLsForDefaultMatch(UserTextFromDisplayText(GetText()),
                                  GetDesiredTLD(), transition,
                                  is_history_what_you_typed_match,
                                  alternate_nav_url);
}

void AutocompleteEdit::SelectAll(bool reversed) {
  if (reversed)
    SetSelection(GetTextLength(), 0);
  else
    SetSelection(0, GetTextLength());
}

void AutocompleteEdit::RevertAll() {
  ScopedFreeze freeze(this, GetTextObjectModel());
  ClosePopup();
  popup_->manually_selected_match_.Clear();
  SetInputInProgress(false);
  paste_state_ = NONE;
  InternalSetUserText(std::wstring());
  SetWindowText(permanent_text_.c_str());
  keyword_.clear();
  is_keyword_hint_ = false;
  disable_keyword_ui_ = false;
  show_search_hint_ = permanent_text_.empty();
  PlaceCaretAt(has_focus_ ? permanent_text_.length() : 0);
  saved_selection_for_focus_change_.cpMin = -1;
  has_temporary_text_ = false;
  TextChanged();
}

void AutocompleteEdit::AcceptInput(WindowOpenDisposition disposition,
                                   bool for_drop) {
  // Get the URL and transition type for the selected entry.
  PageTransition::Type transition;
  bool is_history_what_you_typed_match;
  std::wstring alternate_nav_url;
  const std::wstring url(GetURLForCurrentText(&transition,
                                              &is_history_what_you_typed_match,
                                              &alternate_nav_url));
  if (url.empty())
    return;

  if (url == permanent_text_) {
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

  OpenURL(url, disposition, transition, alternate_nav_url,
          AutocompletePopup::kNoMatch,
          is_keyword_hint_ ? std::wstring() : keyword_);
}

void AutocompleteEdit::OpenURL(const std::wstring& url,
                               WindowOpenDisposition disposition,
                               PageTransition::Type transition,
                               const std::wstring& alternate_nav_url,
                               size_t selected_line,
                               const std::wstring& keyword) {
  if (url.empty())
    return;

  ScopedFreeze freeze(this, GetTextObjectModel());
  SendOpenNotification(selected_line, keyword);

  if (disposition != NEW_BACKGROUND_TAB)
    RevertAll();  // Revert the box to its unedited state
  controller_->OnAutocompleteAccept(url, disposition, transition,
                                    alternate_nav_url);
}

void AutocompleteEdit::ClosePopup() {
  popup_->StopAutocomplete();
}

IAccessible* AutocompleteEdit::GetIAccessible() {
  if (!autocomplete_accessibility_) {
    CComObject<AutocompleteAccessibility>* accessibility = NULL;
    if (!SUCCEEDED(CComObject<AutocompleteAccessibility>::CreateInstance(
            &accessibility)) || !accessibility)
      return NULL;

    // Wrap the created object in a smart pointer so it won't leak.
    CComPtr<IAccessible> accessibility_comptr(accessibility);
    if (!SUCCEEDED(accessibility->Initialize(this)))
      return NULL;

    // Copy to the class smart pointer, and notify that an instance of
    // IAccessible was allocated for m_hWnd.
    autocomplete_accessibility_ = accessibility_comptr;
    NotifyWinEvent(EVENT_OBJECT_CREATE, m_hWnd, OBJID_CLIENT, CHILDID_SELF);
  }
  // Detach to leave ref counting to the caller.
  return autocomplete_accessibility_.Detach();
}

void AutocompleteEdit::SetDropHighlightPosition(int position) {
  if (drop_highlight_position_ != position) {
    RepaintDropHighlight(drop_highlight_position_);
    drop_highlight_position_ = position;
    RepaintDropHighlight(drop_highlight_position_);
  }
}

void AutocompleteEdit::MoveSelectedText(int new_position) {
  const std::wstring selected_text(GetSelectedText());
  CHARRANGE sel;
  GetSel(sel);
  DCHECK((sel.cpMax != sel.cpMin) && (new_position >= 0) &&
         (new_position <= GetTextLength()));

  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();

  // Nuke the selected text.
  ReplaceSel(L"", TRUE);

  // And insert it into the new location.
  if (new_position >= sel.cpMin)
    new_position -= (sel.cpMax - sel.cpMin);
  PlaceCaretAt(new_position);
  ReplaceSel(selected_text.c_str(), TRUE);

  OnAfterPossibleChange();
}

void AutocompleteEdit::InsertText(int position, const std::wstring& text) {
  DCHECK((position >= 0) && (position <= GetTextLength()));
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  SetSelection(position, position);
  ReplaceSel(text.c_str());
  OnAfterPossibleChange();
}

void AutocompleteEdit::PasteAndGo(const std::wstring& text) {
  if (CanPasteAndGo(text))
    PasteAndGo();
}

bool AutocompleteEdit::OverrideAccelerator(
    const ChromeViews::Accelerator& accelerator) {
  // Only override <esc>, and only when there is input in progress -- otherwise,
  // if focus happens to be in the location bar, users can't still hit <esc> to
  // stop a load.
  if ((accelerator.GetKeyCode() != VK_ESCAPE) || accelerator.IsAltDown() ||
      !user_input_in_progress_)
    return false;

  if (!has_temporary_text_ ||
      (popup_->URLsForCurrentSelection(NULL, NULL, NULL) == original_url_)) {
    // The popup isn't open or the selection in it is still the default
    // selection, so revert the box all the way back to its unedited state.
    RevertAll();
    return true;
  }

  // The user typed something, then selected a different item.  Restore the
  // text they typed and change back to the default item.
  // NOTE: This purposefully does not reset paste_state_.
  ScopedFreeze freeze(this, GetTextObjectModel());
  just_deleted_text_ = false;
  const std::wstring new_window_text(user_text_ +
                                     inline_autocomplete_text_);
  SetWindowText(new_window_text.c_str());
  SetSelectionRange(original_selection_);
  has_temporary_text_ = false;
  popup_->manually_selected_match_ = original_selected_match_;
  UpdatePopup();
  TextChanged();
  return true;
}

void AutocompleteEdit::HandleExternalMsg(UINT msg,
                                         UINT flags,
                                         const CPoint& screen_point) {
  if (msg == WM_CAPTURECHANGED) {
    SendMessage(msg, 0, NULL);
    return;
  }

  CPoint client_point(screen_point);
  ::MapWindowPoints(NULL, m_hWnd, &client_point, 1);
  SendMessage(msg, flags, MAKELPARAM(client_point.x, client_point.y));
}

void AutocompleteEdit::OnPopupDataChanged(
    const std::wstring& text,
    bool is_temporary_text,
    const AutocompleteResult::Selection& previous_selected_match,
    const std::wstring& keyword,
    bool is_keyword_hint,
    bool can_show_search_hint) {
  // We don't want to show the search hint if we're showing a keyword hint or
  // selected keyword, or (subtle!) if we would be showing a selected keyword
  // but for disable_keyword_ui_.
  can_show_search_hint &= keyword.empty();

  // Update keyword/hint-related local state.
  bool keyword_state_changed = (keyword_ != keyword) ||
      ((is_keyword_hint_ != is_keyword_hint) && !keyword.empty()) ||
      (show_search_hint_ != can_show_search_hint);
  if (keyword_state_changed) {
    keyword_ = keyword;
    is_keyword_hint_ = is_keyword_hint;
    show_search_hint_ = can_show_search_hint;
  }

  // Handle changes to temporary text.
  if (is_temporary_text) {
    if (!has_temporary_text_) {
      // Save the original selection and URL so it can be reverted later.
      has_temporary_text_ = true;
      GetSelection(original_selection_);
      original_url_ = popup_->URLsForCurrentSelection(NULL, NULL, NULL);
      original_selected_match_ = previous_selected_match;
    }

    // Set new text and cursor position.  Sometimes this does extra work (e.g.
    // when the new text and the old text are identical), but it's only called
    // when the user manually changes the selected line in the popup, so that's
    // not really a problem.  Also, even when the text hasn't changed we'd want
    // to update the caret, because if the user had the cursor in the middle of
    // the text and then arrowed to another entry with the same text, we'd still
    // want to move the caret.
    const std::wstring display_text(DisplayTextFromUserText(text));
    ScopedFreeze freeze(this, GetTextObjectModel());
    SetWindowText(display_text.c_str());
    PlaceCaretAt(display_text.length());
    TextChanged();
    return;
  }

  // Handle changes to inline autocomplete text.  Don't make changes if the user
  // is showing temporary text.  Making display changes would be obviously
  // wrong; making changes to the inline_autocomplete_text_ itself turns out to
  // be more subtlely wrong, because it means hitting esc will no longer revert
  // to the original state before arrowing.
  if (!has_temporary_text_) {
    inline_autocomplete_text_ = text;
    // Update the text and selection.  Because this can be called repeatedly
    // while typing, we've careful not to freeze the edit unless we really need
    // to.  Also, unlike in the temporary text case above, here we don't want to
    // update the caret/selection unless we have to, since this might make the
    // user's caret position change without warning during typing.
    const std::wstring display_text(
        DisplayTextFromUserText(user_text_ + inline_autocomplete_text_));
    if (display_text != GetText()) {
      ScopedFreeze freeze(this, GetTextObjectModel());
      SetWindowText(display_text.c_str());
      // Set a reversed selection to keep the caret in the same position, which
      // avoids scrolling the user's text.
      SetSelection(
          static_cast<LONG>(display_text.length()),
          static_cast<LONG>(DisplayTextFromUserText(user_text_).length()));
      TextChanged();
      return;
    }
  }

  // If the above changes didn't warrant a text update but we did change keyword
  // state, we have yet to notify the controller about it.
  if (keyword_state_changed)
    controller_->OnChanged();
}

bool AutocompleteEdit::IsCommandEnabled(int id) const {
  switch (id) {
    case IDS_UNDO:         return !!CanUndo();
    case IDS_CUT:          return !!CanCut();
    case IDS_COPY:         return !!CanCopy();
    case IDS_PASTE:        return !!CanPaste();
    case IDS_PASTE_AND_GO: return CanPasteAndGo(GetClipboardText());
    case IDS_SELECTALL:    return !!CanSelectAll();
    case IDS_EDIT_SEARCH_ENGINES:
      return command_controller_->IsCommandEnabled(IDC_EDIT_SEARCH_ENGINES);
    default:               NOTREACHED(); return false;
  }
}

bool AutocompleteEdit::GetContextualLabel(int id, std::wstring* out) const {
  if ((id != IDS_PASTE_AND_GO) ||
      // No need to change the default IDS_PASTE_AND_GO label for a typed
      // destination (this is also the type set when Paste And Go is disabled).
      (paste_and_go_transition_ == PageTransition::TYPED))
    return false;

  out->assign(l10n_util::GetString(IDS_PASTE_AND_SEARCH));
  return true;
}

void AutocompleteEdit::ExecuteCommand(int id) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  if (id == IDS_PASTE_AND_GO) {
    // This case is separate from the switch() below since we don't want to wrap
    // it in OnBefore/AfterPossibleChange() calls.
    PasteAndGo();
    return;
  }

  OnBeforePossibleChange();
  switch (id) {
    case IDS_UNDO:
      Undo();
      break;

    case IDS_CUT:
      Cut();
      break;

    case IDS_COPY:
      Copy();
      break;

    case IDS_PASTE:
      Paste();
      break;

    case IDS_SELECTALL:
      SelectAll(false);
      break;

    case IDS_EDIT_SEARCH_ENGINES:
      command_controller_->ExecuteCommand(IDC_EDIT_SEARCH_ENGINES);
      break;

    default:
      NOTREACHED();
      break;
  }
  OnAfterPossibleChange();
}

// static
int CALLBACK AutocompleteEdit::WordBreakProc(LPTSTR edit_text,
                                             int current_pos,
                                             int num_bytes,
                                             int action) {
  // TODO(pkasting): http://b/1111308 We should let other people, like ICU and
  // GURL, do the work for us here instead of writing all this ourselves.

  // Sadly, even though the MSDN docs claim that the third parameter here is a
  // number of characters, they lie.  It's a number of bytes.
  const int length = num_bytes / sizeof(wchar_t);

  // With no clear guidance from the MSDN docs on how to handle "not found" in
  // the "find the nearest xxx..." cases below, I cap the return values at
  // [0, length].  Since one of these (0) is also a valid position, the return
  // values are thus ambiguous :(
  switch (action) {
    // Find nearest character before current position that begins a word.
    case WB_LEFT:
    case WB_MOVEWORDLEFT: {
      if (current_pos < 2) {
        // Either current_pos == 0, so we have a "not found" case and return 0,
        // or current_pos == 1, and the only character before this position is
        // at 0.
        return 0;
      }

      // Look for a delimiter before the previous character; the previous word
      // starts immediately after.  (If we looked for a delimiter before the
      // current character, we could stop on the immediate prior character,
      // which would mean we'd return current_pos -- which isn't "before the
      // current position".)
      const int prev_delim =
          WordBreakProc(edit_text, current_pos - 1, num_bytes, WB_LEFTBREAK);

      if ((prev_delim == 0) &&
          !WordBreakProc(edit_text, 0, num_bytes, WB_ISDELIMITER)) {
        // Got back 0, but position 0 isn't a delimiter.  This was a "not
        // found" 0, so return one of our own.
        return 0;
      }

      return prev_delim + 1;
    }

    // Find nearest character after current position that begins a word.
    case WB_RIGHT:
    case WB_MOVEWORDRIGHT: {
      if (WordBreakProc(edit_text, current_pos, num_bytes, WB_ISDELIMITER)) {
        // The current character is a delimiter, so the next character starts
        // a new word.  Done.
        return current_pos + 1;
      }

      // Look for a delimiter after the current character; the next word starts
      // immediately after.
      const int next_delim =
          WordBreakProc(edit_text, current_pos, num_bytes, WB_RIGHTBREAK);
      if (next_delim == length) {
        // Didn't find a delimiter.  Return length to signal "not found".
        return length;
      }

      return next_delim + 1;
    }

    // Determine if the current character delimits words.
    case WB_ISDELIMITER:
      return !!(WordBreakProc(edit_text, current_pos, num_bytes, WB_CLASSIFY) &
                WBF_BREAKLINE);

    // Return the classification of the current character.
    case WB_CLASSIFY:
      if (IsWhitespace(edit_text[current_pos])) {
        // Whitespace normally breaks words, but the MSDN docs say that we must
        // not break on the CRs in a "CR, LF" or a "CR, CR, LF" sequence.  Just
        // check for an arbitrarily long sequence of CRs followed by LF and
        // report "not a delimiter" for the current CR in that case.
        while ((current_pos < (length - 1)) &&
               (edit_text[current_pos] == 0x13)) {
          if (edit_text[++current_pos] == 0x10)
            return WBF_ISWHITE;
        }
        return WBF_BREAKLINE | WBF_ISWHITE;
      }

      // Punctuation normally breaks words, but the first two characters in
      // "://" (end of scheme) should not be breaks, so that "http://" will be
      // treated as one word.
      if (ispunct(edit_text[current_pos], std::locale()) &&
          !SchemeEnd(edit_text, current_pos, length) &&
          !SchemeEnd(edit_text, current_pos - 1, length))
        return WBF_BREAKLINE;

      // Normal character, no flags.
      return 0;

    // Finds nearest delimiter before current position.
    case WB_LEFTBREAK:
      for (int i = current_pos - 1; i >= 0; --i) {
        if (WordBreakProc(edit_text, i, num_bytes, WB_ISDELIMITER))
          return i;
      }
      return 0;

    // Finds nearest delimiter after current position.
    case WB_RIGHTBREAK:
      for (int i = current_pos + 1; i < length; ++i) {
        if (WordBreakProc(edit_text, i, num_bytes, WB_ISDELIMITER))
          return i;
      }
      return length;
  }

  NOTREACHED();
  return 0;
}

// static
bool AutocompleteEdit::SchemeEnd(LPTSTR edit_text,
                                 int current_pos,
                                 int length) {
  return (current_pos >= 0) &&
         ((length - current_pos) > 2) &&
         (edit_text[current_pos] == ':') &&
         (edit_text[current_pos + 1] == '/') &&
         (edit_text[current_pos + 2] == '/');
}

// static
HDC AutocompleteEdit::BeginPaintIntercept(HWND hWnd, LPPAINTSTRUCT lpPaint) {
  if (!edit_hwnd || (hWnd != edit_hwnd))
    return ::BeginPaint(hWnd, lpPaint);

  *lpPaint = paint_struct;
  return paint_struct.hdc;
}

// static
BOOL AutocompleteEdit::EndPaintIntercept(HWND hWnd,
                                         CONST PAINTSTRUCT* lpPaint) {
  return (edit_hwnd && (hWnd == edit_hwnd)) ?
      true : ::EndPaint(hWnd, lpPaint);
}

void AutocompleteEdit::OnChar(TCHAR ch, UINT repeat_count, UINT flags) {
  // Don't let alt-enter beep.  Not sure this is necessary, as the standard
  // alt-enter will hit DiscardWMSysChar() and get thrown away, and
  // ctrl-alt-enter doesn't seem to reach here for some reason?  At least not on
  // my system... still, this is harmless and maybe necessary in other locales.
  if (ch == VK_RETURN && (flags & KF_ALTDOWN))
    return;

  // Escape is processed in OnKeyDown.  Don't let any WM_CHAR messages propagate
  // as we don't want the RichEdit to do anything funky.
  if (ch == VK_ESCAPE && !(flags & KF_ALTDOWN))
    return;

  if (ch == VK_TAB) {
    // Don't add tabs to the input.
    return;
  }

  HandleKeystroke(GetCurrentMessage()->message, ch, repeat_count, flags);
}

void AutocompleteEdit::OnContextMenu(HWND window, const CPoint& point) {
  if (point.x == -1 || point.y == -1) {
    POINT p;
    GetCaretPos(&p);
    MapWindowPoints(HWND_DESKTOP, &p, 1);
    context_menu_->RunMenuAt(p.x, p.y);
  } else {
    context_menu_->RunMenuAt(point.x, point.y);
  }
}

void AutocompleteEdit::OnCopy() {
  const std::wstring text(GetSelectedText());
  if (text.empty())
    return;

  ClipboardService* clipboard = g_browser_process->clipboard_service();
  clipboard->Clear();
  clipboard->WriteText(text);

  // Check if the user is copying the whole address bar.  If they are, we
  // assume they are trying to copy a URL and write this to the clipboard as a
  // hyperlink.
  if (static_cast<int>(text.length()) < GetTextLength())
    return;

  // The entire control is selected.  Let's see what the user typed.  Usually
  // we'd use GetDesiredTLD() to figure out the TLD, but right now the user is
  // probably holding down control to cause the copy (which would make us always
  // think the user wanted ".com" added).
  url_parse::Parsed parts;
  const AutocompleteInput::Type type = AutocompleteInput::Parse(
      UserTextFromDisplayText(text), std::wstring(), &parts, NULL);
  if (type == AutocompleteInput::URL) {
    const GURL url(URLFixerUpper::FixupURL(text, std::wstring()));
    clipboard->WriteHyperlink(text, url.spec());
  }
}

void AutocompleteEdit::OnCut() {
  OnCopy();

  // This replace selection will have no effect (even on the undo stack) if the
  // current selection is empty.
  ReplaceSel(L"", true);
}

LRESULT AutocompleteEdit::OnGetObject(UINT uMsg, WPARAM wparam, LPARAM lparam) {
  // Accessibility readers will send an OBJID_CLIENT message.
  if (lparam == OBJID_CLIENT) {
    // Re-attach for internal re-usage of accessibility pointer.
    autocomplete_accessibility_.Attach(GetIAccessible());

    if (autocomplete_accessibility_) {
      return LresultFromObject(IID_IAccessible, wparam,
                               autocomplete_accessibility_.p);
    }
  }
  return 0;
}

LRESULT AutocompleteEdit::OnImeComposition(UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  LRESULT result = DefWindowProc(message, wparam, lparam);
  if (!OnAfterPossibleChange() && (lparam & GCS_RESULTSTR)) {
    // The result string changed, but the text in the popup didn't actually
    // change.  This means the user finalized the composition.  Rerun
    // autocomplete so that we can now trigger inline autocomplete if
    // applicable.
    //
    // Note that if we're in the midst of losing focus, UpdatePopup() won't
    // actually rerun autocomplete, but will just set local state correctly.
    UpdatePopup();
  }
  return result;
}

void AutocompleteEdit::OnKeyDown(TCHAR key, UINT repeat_count, UINT flags) {
  if (OnKeyDownAllModes(key, repeat_count, flags) || popup_window_mode_ ||
      OnKeyDownOnlyWritable(key, repeat_count, flags))
    return;

  // CRichEditCtrl changes its text on WM_KEYDOWN instead of WM_CHAR for many
  // different keys (backspace, ctrl-v, ...), so we call this in both cases.
  HandleKeystroke(GetCurrentMessage()->message, key, repeat_count, flags);
}

void AutocompleteEdit::OnKeyUp(TCHAR key, UINT repeat_count, UINT flags) {
  if ((key == VK_CONTROL) && (control_key_state_ != UP)) {
    control_key_state_ = UP;
    if (popup_->is_open()) {
      // Autocomplete history provider results may change, so refresh the
      // popup.  This will force user_input_in_progress_ to true, but if the
      // popup is open, that should have already been the case.
      UpdatePopup();
    }
  }

  SetMsgHandled(false);
}

void AutocompleteEdit::OnKillFocus(HWND focus_wnd) {
  if (m_hWnd == focus_wnd) {
    // Focus isn't actually leaving.
    SetMsgHandled(false);
    return;
  }

  has_focus_ = false;
  control_key_state_ = UP;
  paste_state_ = NONE;

  // Close the popup.
  ClosePopup();

  // Save the user's existing selection to restore it later.
  GetSelection(saved_selection_for_focus_change_);

  // Like typing, killing focus "accepts" the temporary text as the user
  // text, because it makes little sense to have temporary text when the
  // popup is closed.
  InternalSetUserText(UserTextFromDisplayText(GetText()));
  has_temporary_text_ = false;

  // Let the CRichEditCtrl do its default handling.  This will complete any
  // in-progress IME composition.  We must do this after setting has_focus_ to
  // false so that UpdatePopup() will know not to rerun autocomplete.
  ScopedFreeze freeze(this, GetTextObjectModel());
  DefWindowProc(WM_KILLFOCUS, reinterpret_cast<WPARAM>(focus_wnd), 0);

  // Hide the "Type to search" hint if necessary.  We do this after calling
  // DefWindowProc() because processing the resulting IME messages may notify
  // the controller that input is in progress, which could cause the visible
  // hints to change.  (I don't know if there's a real scenario where they
  // actually do change, but this is safest.)
  if (show_search_hint_ || (is_keyword_hint_ && !keyword_.empty()))
    controller_->OnChanged();

  // Cancel any user selection and scroll the text back to the beginning of the
  // URL.  We have to do this after calling DefWindowProc() because otherwise
  // an in-progress IME composition will be completed at the new caret position,
  // resulting in the string jumping unexpectedly to the front of the edit.
  PlaceCaretAt(0);
}

void AutocompleteEdit::OnLButtonDblClk(UINT keys, const CPoint& point) {
  // Save the double click info for later triple-click detection.
  tracking_double_click_ = true;
  double_click_point_ = point;
  double_click_time_ = GetCurrentMessage()->time;
  possible_drag_ = false;

  // Modifying the selection counts as accepting any inline autocompletion, so
  // track "changes" made by clicking the mouse button.
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(WM_LBUTTONDBLCLK, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, false), point.y));
  OnAfterPossibleChange();

  gaining_focus_.reset();  // See NOTE in OnMouseActivate().
}

void AutocompleteEdit::OnLButtonDown(UINT keys, const CPoint& point) {
  if (gaining_focus_.get()) {
    // This click is giving us focus, so we need to track how much the mouse
    // moves to see if it's a drag or just a click. Clicks should select all
    // the text.
    tracking_click_ = true;
    mouse_down_point_ = point;

    // When Chrome was already the activated app, we haven't reached
    // OnSetFocus() yet.  When we get there, don't restore the saved selection,
    // since it will just screw up the user's interaction with the edit.
    saved_selection_for_focus_change_.cpMin = -1;

    // Crazy hack: In this particular case, the CRichEditCtrl seems to have an
    // internal flag that discards the next WM_LBUTTONDOWN without processing
    // it, so that clicks on the edit when its owning app is not activated are
    // eaten rather than processed (despite whatever the return value of
    // DefWindowProc(WM_MOUSEACTIVATE, ...) may say).  This behavior is
    // confusing and we want the click to be treated normally.  So, to reset the
    // CRichEditCtrl's internal flag, we pass it an extra WM_LBUTTONDOWN here
    // (as well as a matching WM_LBUTTONUP, just in case we'd be confusing some
    // kind of state tracking otherwise).
    DefWindowProc(WM_LBUTTONDOWN, keys, MAKELPARAM(point.x, point.y));
    DefWindowProc(WM_LBUTTONUP, keys, MAKELPARAM(point.x, point.y));
  }

  // Check for triple click, then reset tracker.  Should be safe to subtract
  // double_click_time_ from the current message's time even if the timer has
  // wrapped in between.
  const bool is_triple_click = tracking_double_click_ &&
      win_util::IsDoubleClick(double_click_point_, point,
                              GetCurrentMessage()->time - double_click_time_);
  tracking_double_click_ = false;

  if (!gaining_focus_.get() && !is_triple_click)
    OnPossibleDrag(point);


  // Modifying the selection counts as accepting any inline autocompletion, so
  // track "changes" made by clicking the mouse button.
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(WM_LBUTTONDOWN, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, is_triple_click),
                           point.y));
  OnAfterPossibleChange();

  gaining_focus_.reset();
}

void AutocompleteEdit::OnLButtonUp(UINT keys, const CPoint& point) {
  // default processing should happen first so we can see the result of the
  // selection
  ScopedFreeze freeze(this, GetTextObjectModel());
  DefWindowProc(WM_LBUTTONUP, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, false), point.y));

  // When the user has clicked and released to give us focus, select all.
  if (tracking_click_ && !win_util::IsDrag(mouse_down_point_, point)) {
    // Select all in the reverse direction so as not to scroll the caret
    // into view and shift the contents jarringly.
    SelectAll(true);
    possible_drag_ = false;
  }

  tracking_click_ = false;

  UpdateDragDone(keys);
}

LRESULT AutocompleteEdit::OnMouseActivate(HWND window,
                                          UINT hit_test,
                                          UINT mouse_message) {
  // First, give other handlers a chance to handle the message to see if we are
  // actually going to activate and gain focus.
  LRESULT result = DefWindowProc(WM_MOUSEACTIVATE,
                                 reinterpret_cast<WPARAM>(window),
                                 MAKELPARAM(hit_test, mouse_message));
  // Check if we're getting focus from a left click.  We have to do this here
  // rather than in OnLButtonDown() since in many scenarios OnSetFocus() will be
  // reached before OnLButtonDown(), preventing us from detecting this properly
  // there.  Also in those cases, we need to already know in OnSetFocus() that
  // we should not restore the saved selection.
  if (!has_focus_ && (mouse_message == WM_LBUTTONDOWN) &&
      (result == MA_ACTIVATE)) {
    DCHECK(!gaining_focus_.get());
    gaining_focus_.reset(new ScopedFreeze(this, GetTextObjectModel()));
    // NOTE: Despite |mouse_message| being WM_LBUTTONDOWN here, we're not
    // guaranteed to call OnLButtonDown() later!  Specifically, if this is the
    // second click of a double click, we'll reach here but later call
    // OnLButtonDblClk().  Make sure |gaining_focus_| gets reset both places, or
    // we'll have visual glitchiness and then DCHECK failures.

    // Don't restore saved selection, it will just screw up our interaction
    // with this edit.
    saved_selection_for_focus_change_.cpMin = -1;
  }
  return result;
}

void AutocompleteEdit::OnMouseMove(UINT keys, const CPoint& point) {
  if (possible_drag_) {
    StartDragIfNecessary(point);
    // Don't fall through to default mouse handling, otherwise a second
    // drag session may start.
    return;
  }

  if (tracking_click_ && !win_util::IsDrag(mouse_down_point_, point))
    return;

  tracking_click_ = false;

  // Return quickly if this can't change the selection/cursor, so we don't
  // create a ScopedFreeze (and thus do an UpdateWindow()) on every
  // WM_MOUSEMOVE.
  if (!(keys & MK_LBUTTON)) {
    DefWindowProc(WM_MOUSEMOVE, keys, MAKELPARAM(point.x, point.y));
    return;
  }

  // Clamp the selection to the visible text so the user can't drag to select
  // the "phantom newline".  In theory we could achieve this by clipping the X
  // coordinate, but in practice the edit seems to behave nondeterministically
  // with similar sequences of clipped input coordinates fed to it.  Maybe it's
  // reading the mouse cursor position directly?
  //
  // This solution has a minor visual flaw, however: if there's a visible cursor
  // at the edge of the text (only true when there's no selection), dragging the
  // mouse around outside that edge repaints the cursor on every WM_MOUSEMOVE
  // instead of allowing it to blink normally.  To fix this, we special-case
  // this exact case and discard the WM_MOUSEMOVE messages instead of passing
  // them along.
  //
  // But even this solution has a flaw!  (Argh.)  In the case where the user has
  // a selection that starts at the edge of the edit, and proceeds to the middle
  // of the edit, and the user is dragging back past the start edge to remove
  // the selection, there's a redraw problem where the change between having the
  // last few bits of text still selected and having nothing selected can be
  // slow to repaint (which feels noticeably strange).  This occurs if you only
  // let the edit receive a single WM_MOUSEMOVE past the edge of the text.  I
  // think on each WM_MOUSEMOVE the edit is repainting its previous state, then
  // updating its internal variables to the new state but not repainting.  To
  // fix this, we allow one more WM_MOUSEMOVE through after the selection has
  // supposedly been shrunk to nothing; this makes the edit redraw the selection
  // quickly so it feels smooth.
  CHARRANGE selection;
  GetSel(selection);
  const bool possibly_can_discard_mousemove =
      (selection.cpMin == selection.cpMax) &&
      (((selection.cpMin == 0) &&
        (ClipXCoordToVisibleText(point.x, false) > point.x)) ||
       ((selection.cpMin == GetTextLength()) &&
        (ClipXCoordToVisibleText(point.x, false) < point.x)));
  if (!can_discard_mousemove_ || !possibly_can_discard_mousemove) {
    can_discard_mousemove_ = possibly_can_discard_mousemove;
    ScopedFreeze freeze(this, GetTextObjectModel());
    OnBeforePossibleChange();
    // Force the Y coordinate to the center of the clip rect.  The edit
    // behaves strangely when the cursor is dragged vertically: if the cursor
    // is in the middle of the text, drags inside the clip rect do nothing,
    // and drags outside the clip rect act as if the cursor jumped to the
    // left edge of the text.  When the cursor is at the right edge, drags of
    // just a few pixels vertically end up selecting the "phantom newline"...
    // sometimes.
    RECT r;
    GetRect(&r);
    DefWindowProc(WM_MOUSEMOVE, keys,
                  MAKELPARAM(point.x, (r.bottom - r.top) / 2));
    OnAfterPossibleChange();
  }
}

void AutocompleteEdit::OnPaint(HDC bogus_hdc) {
  // We need to paint over the top of the edit.  If we simply let the edit do
  // its default painting, then do ours into the window DC, the screen is
  // updated in between and we can get flicker.  To avoid this, we force the
  // edit to paint into a memory DC, which we also paint onto, then blit the
  // whole thing to the screen.

  // Don't paint if not necessary.
  CRect paint_clip_rect;
  if (!GetUpdateRect(&paint_clip_rect, true))
    return;

  // Begin painting, and create a memory DC for the edit to paint into.
  CPaintDC paint_dc(m_hWnd);
  CDC memory_dc(CreateCompatibleDC(paint_dc));
  CRect rect;
  GetClientRect(&rect);
  // NOTE: This next call uses |paint_dc| instead of |memory_dc| because
  // |memory_dc| contains a 1x1 monochrome bitmap by default, which will cause
  // |memory_bitmap| to be monochrome, which isn't what we want.
  CBitmap memory_bitmap(CreateCompatibleBitmap(paint_dc, rect.Width(),
                                               rect.Height()));
  HBITMAP old_bitmap = memory_dc.SelectBitmap(memory_bitmap);

  // Tell our intercept functions to supply our memory DC to the edit when it
  // tries to call BeginPaint().
  //
  // The sane way to do this would be to use WM_PRINTCLIENT to ask the edit to
  // paint into our desired DC.  Unfortunately, the Rich Edit 3.0 that ships
  // with Windows 2000/XP/Vista doesn't handle WM_PRINTCLIENT correctly; it
  // treats it just like WM_PAINT and calls BeginPaint(), ignoring our provided
  // DC.  The Rich Edit 6.0 that ships with Office 2007 handles this better, but
  // has other issues, and we can't redistribute that DLL anyway.  So instead,
  // we use this scary hack.
  //
  // NOTE: It's possible to get nested paint calls (!) (try setting the
  // permanent URL to something longer than the edit width, then selecting the
  // contents of the edit, typing a character, and hitting <esc>), so we can't
  // DCHECK(!edit_hwnd_) here.  Instead, just save off the old HWND, which most
  // of the time will be NULL.
  HWND old_edit_hwnd = edit_hwnd;
  edit_hwnd = m_hWnd;
  paint_struct = paint_dc.m_ps;
  paint_struct.hdc = memory_dc;
  DefWindowProc(WM_PAINT, reinterpret_cast<WPARAM>(bogus_hdc), 0);

  // Make the selection look better.
  EraseTopOfSelection(&memory_dc, rect, paint_clip_rect);

  // Draw a slash through the scheme if this is insecure.
  if (insecure_scheme_component_.is_nonempty())
    DrawSlashForInsecureScheme(memory_dc, rect, paint_clip_rect);

  // Draw the drop highlight.
  if (drop_highlight_position_ != -1)
    DrawDropHighlight(memory_dc, rect, paint_clip_rect);

  // Blit the memory DC to the actual paint DC and clean up.
  BitBlt(paint_dc, rect.left, rect.top, rect.Width(), rect.Height(), memory_dc,
         rect.left, rect.top, SRCCOPY);
  memory_dc.SelectBitmap(old_bitmap);
  edit_hwnd = old_edit_hwnd;
}

void AutocompleteEdit::OnNonLButtonDown(UINT keys, const CPoint& point) {
  // Interestingly, the edit doesn't seem to cancel triple clicking when the
  // x-buttons (which usually means "thumb buttons") are pressed, so we only
  // call this for M and R down.
  tracking_double_click_ = false;

  OnPossibleDrag(point);

  SetMsgHandled(false);
}

void AutocompleteEdit::OnNonLButtonUp(UINT keys, const CPoint& point) {
  UpdateDragDone(keys);

  // Let default handler have a crack at this.
  SetMsgHandled(false);
}

void AutocompleteEdit::OnPaste() {
  // Replace the selection if we have something to paste.
  const std::wstring text(GetClipboardText());
  if (!text.empty()) {
    // If this paste will be replacing all the text, record that, so we can do
    // different behaviors in such a case.
    CHARRANGE sel;
    GetSel(sel);
    if (IsSelectAll(sel))
      paste_state_ = REPLACING_ALL;
    ReplaceSel(text.c_str(), true);
  }
}

void AutocompleteEdit::OnSetFocus(HWND focus_wnd) {
  has_focus_ = true;
  control_key_state_ = (GetKeyState(VK_CONTROL) < 0) ? DOWN_WITHOUT_CHANGE : UP;

  // Notify controller if it needs to show hint UI of some kind.
  ScopedFreeze freeze(this, GetTextObjectModel());
  if (show_search_hint_ || (is_keyword_hint_ && !keyword_.empty()))
    controller_->OnChanged();

  // Restore saved selection if available.
  if (saved_selection_for_focus_change_.cpMin != -1) {
    SetSelectionRange(saved_selection_for_focus_change_);
    saved_selection_for_focus_change_.cpMin = -1;
  }

  SetMsgHandled(false);
}

void AutocompleteEdit::OnSysChar(TCHAR ch, UINT repeat_count, UINT flags) {
  // Nearly all alt-<xxx> combos result in beeping rather than doing something
  // useful, so we discard most.  Exceptions:
  //   * ctrl-alt-<xxx>, which is sometimes important, generates WM_CHAR instead
  //     of WM_SYSCHAR, so it doesn't need to be handled here.
  //   * alt-space gets translated by the default WM_SYSCHAR handler to a
  //     WM_SYSCOMMAND to open the application context menu, so we need to allow
  //     it through.
  if (ch == VK_SPACE)
    SetMsgHandled(false);
}

void AutocompleteEdit::HandleKeystroke(UINT message, TCHAR key,
                                       UINT repeat_count, UINT flags) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(message, key, MAKELPARAM(repeat_count, flags));
  OnAfterPossibleChange();
}

bool AutocompleteEdit::OnKeyDownOnlyWritable(TCHAR key,
                                             UINT repeat_count,
                                             UINT flags) {
  // NOTE: Annoyingly, ctrl-alt-<key> generates WM_KEYDOWN rather than
  // WM_SYSKEYDOWN, so we need to check (flags & KF_ALTDOWN) in various places
  // in this function even with a WM_SYSKEYDOWN handler.

  int count = repeat_count;
  switch (key) {
    case VK_RETURN:
      AcceptInput((flags & KF_ALTDOWN) ? NEW_FOREGROUND_TAB : CURRENT_TAB,
                  false);
      return true;

    case VK_UP:
      count = -count;
      // FALL THROUGH
    case VK_DOWN:
      if (flags & KF_ALTDOWN)
        return false;

      // NOTE: VK_DOWN/VK_UP purposefully don't trigger any code that resets
      // paste_state_.
      disable_keyword_ui_ = false;
      if (!popup_->is_open()) {
        if (!popup_->query_in_progress()) {
          // The popup is neither open nor working on a query already.  So,
          // start an autocomplete query for the current text.  This also sets
          // user_input_in_progress_ to true, which we want: if the user has
          // started to interact with the popup, changing the permanent_text_
          // shouldn't change the displayed text.
          // Note: This does not force the popup to open immediately.
          if (!user_input_in_progress_)
            InternalSetUserText(permanent_text_);
          DCHECK(user_text_ == UserTextFromDisplayText(GetText()));
          UpdatePopup();
        }

        // Now go ahead and force the popup to open, and copy the text of the
        // default item into the edit.  We ignore |count|, since without the
        // popup open, the user doesn't really know what they're interacting
        // with.  Since the user hit an arrow key to explicitly open the popup,
        // we assume that they prefer the temporary text of the default item
        // to their own text, like we do when they arrow around an already-open
        // popup.  In many cases the existing text in the edit and the new text
        // will be the same, and the only visible effect will be to cancel any
        // selection and place the cursor at the end of the edit.
        popup_->Move(0);
      } else {
        // The popup is open, so the user should be able to interact with it
        // normally.
        popup_->Move(count);
      }
      return true;

    // Hijacking Editing Commands
    //
    // We hijack the keyboard short-cuts for Cut, Copy, and Paste here so that
    // they go through our clipboard routines.  This allows us to be smarter
    // about how we interact with the clipboard and avoid bugs in the
    // CRichEditCtrl.  If we didn't hijack here, the edit control would handle
    // these internally with sending the WM_CUT, WM_COPY, or WM_PASTE messages.
    //
    // Cut:   Shift-Delete and Ctrl-x are treated as cut.  Ctrl-Shift-Delete and
    //        Ctrl-Shift-x are not treated as cut even though the underlying
    //        CRichTextEdit would treat them as such.
    // Copy:  Ctrl-c is treated as copy.  Shift-Ctrl-c is not.  (This is handled
    //        in OnKeyDownAllModes().)
    // Paste: Shift-Insert and Ctrl-v are tread as paste.  Ctrl-Shift-Insert and
    //        Ctrl-Shift-v are not.
    //
    // This behavior matches most, but not all Windows programs, and largely
    // conforms to what users expect.

    case VK_DELETE:
      if ((flags & KF_ALTDOWN) || GetKeyState(VK_SHIFT) >= 0)
        return false;
      if (control_key_state_ == UP) {
        // Cut text if possible.
        CHARRANGE selection;
        GetSel(selection);
        if (selection.cpMin != selection.cpMax) {
          ScopedFreeze freeze(this, GetTextObjectModel());
          OnBeforePossibleChange();
          Cut();
          OnAfterPossibleChange();
        } else if (popup_->is_open()) {
          // This is a bit overloaded, but we hijack Shift-Delete in this
          // case to delete the current item from the pop-up.  We prefer cutting
          // to this when possible since that's the behavior more people expect
          // from Shift-Delete, and it's more commonly useful.
          popup_->TryDeletingCurrentItem();
        }
      }
      return true;

    case 'X':
      if ((flags & KF_ALTDOWN) || (control_key_state_ == UP))
        return false;
      if (GetKeyState(VK_SHIFT) >= 0) {
        ScopedFreeze freeze(this, GetTextObjectModel());
        OnBeforePossibleChange();
        Cut();
        OnAfterPossibleChange();
      }
      return true;

    case VK_INSERT:
    case 'V':
      if ((flags & KF_ALTDOWN) || ((key == 'V') ?
              (control_key_state_ == UP) : (GetKeyState(VK_SHIFT) >= 0)))
        return false;
      if ((key == 'V') ?
              (GetKeyState(VK_SHIFT) >= 0) : (control_key_state_ == UP)) {
        ScopedFreeze freeze(this, GetTextObjectModel());
        OnBeforePossibleChange();
        Paste();
        OnAfterPossibleChange();
      }
      return true;

    case VK_BACK: {
      if ((flags & KF_ALTDOWN) || is_keyword_hint_ || keyword_.empty())
        return false;

      {
        CHARRANGE selection;
        GetSel(selection);
        if ((selection.cpMin != selection.cpMax) || (selection.cpMin != 0))
          return false;
      }

      // We're showing a keyword and the user pressed backspace at the beginning
      // of the text. Delete the trailing space from the keyword forcing the
      // selected keyword to become empty.
      ScopedFreeze freeze(this, GetTextObjectModel());
      OnBeforePossibleChange();
      const std::wstring window_text(keyword_ + GetText());
      SetWindowText(window_text.c_str());
      PlaceCaretAt(keyword_.length());
      popup_->manually_selected_match_.Clear();
      keyword_.clear();
      OnAfterPossibleChange();
      just_deleted_text_ = true;  // OnAfterPossibleChange() fails to clear this
                                  // since the edit contents have actually grown
                                  // longer.
      return true;
    }

    case VK_TAB: {
      if (is_keyword_hint_ && !keyword_.empty()) {
        // Accept the keyword.
        ScopedFreeze freeze(this, GetTextObjectModel());
        OnBeforePossibleChange();
        SetWindowText(L"");
        popup_->manually_selected_match_.Clear();
        popup_->manually_selected_match_.provider_affinity =
            popup_->autocomplete_controller()->keyword_provider();
        is_keyword_hint_ = false;
        disable_keyword_ui_ = false;
        OnAfterPossibleChange();
        just_deleted_text_ = false;  // OnAfterPossibleChange() erroneously sets
                                     // this since the edit contents have
                                     // disappeared.  It doesn't really matter,
                                     // but we clear it to be consistent.

        // Send out notification (primarily for logging).
        UserMetrics::RecordAction(L"AcceptedKeywordHint", profile_);
      }
      return true;
    }

    case 0xbb:  // Ctrl-'='.  Triggers subscripting (even in plain text mode).
      return true;

    default:
      return false;
  }
}

bool AutocompleteEdit::OnKeyDownAllModes(TCHAR key,
                                         UINT repeat_count,
                                         UINT flags) {
  // See KF_ALTDOWN comment atop OnKeyDownOnlyWriteable().

  switch (key) {
    case VK_CONTROL:
      if (control_key_state_ == UP) {
        control_key_state_ = DOWN_WITHOUT_CHANGE;
        if (popup_->is_open()) {
          DCHECK(!popup_window_mode_);  // How did the popup get open in read-only mode?
          // Autocomplete history provider results may change, so refresh the
          // popup.  This will force user_input_in_progress_ to true, but if the
          // popup is open, that should have already been the case.
          UpdatePopup();
        }
      }
      return false;

    case 'C':
      // See more detailed comments in OnKeyDownOnlyWriteable().
      if ((flags & KF_ALTDOWN) || (control_key_state_ == UP))
        return false;
      if (GetKeyState(VK_SHIFT) >= 0)
        Copy();
      return true;

    default:
      return false;
  }
}

void AutocompleteEdit::OnBeforePossibleChange() {
  // Record our state.
  text_before_change_ = GetText();
  GetSelection(sel_before_change_);
  select_all_before_change_ = IsSelectAll(sel_before_change_);
}

bool AutocompleteEdit::OnAfterPossibleChange() {
  // Prevent the user from selecting the "phantom newline" at the end of the
  // edit.  If they try, we just silently move the end of the selection back to
  // the end of the real text.
  CHARRANGE new_sel;
  GetSelection(new_sel);
  const int length = GetTextLength();
  if ((new_sel.cpMin > length) || (new_sel.cpMax > length)) {
    if (new_sel.cpMin > length)
      new_sel.cpMin = length;
    if (new_sel.cpMax > length)
      new_sel.cpMax = length;
    SetSelectionRange(new_sel);
  }
  const bool selection_differs = (new_sel.cpMin != sel_before_change_.cpMin) ||
      (new_sel.cpMax != sel_before_change_.cpMax);

  // See if the text or selection have changed since OnBeforePossibleChange().
  const std::wstring new_text(GetText());
  const bool text_differs = (new_text != text_before_change_);

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
    if (!text_differs && !popup_->is_open())
      return false;  // Don't open the popup for no reason.
  } else if (!text_differs &&
      (inline_autocomplete_text_.empty() || !selection_differs)) {
    return false;
  }

  const bool had_keyword = !is_keyword_hint_ && !keyword_.empty();

  // Modifying the selection counts as accepting the autocompleted text.
  InternalSetUserText(UserTextFromDisplayText(new_text));
  has_temporary_text_ = false;

  if (text_differs) {
    // When the user has deleted text, don't allow inline autocomplete.  Make
    // sure to not flag cases like selecting part of the text and then pasting
    // (or typing) the prefix of that selection.  (We detect these by making
    // sure the caret, which should be after any insertion, hasn't moved
    // forward of the old selection start.)
    just_deleted_text_ = (text_before_change_.length() > new_text.length()) &&
      (new_sel.cpMin <= std::min(sel_before_change_.cpMin,
                                 sel_before_change_.cpMax));

    // When the user doesn't have a selected keyword, deleting text or replacing
    // all of it with something else should reset the provider affinity.  The
    // typical use case for deleting is that the user starts typing, sees that
    // some entry is close to what he wants, arrows to it, and then deletes some
    // unnecessary bit from the end of the string.  In this case the user didn't
    // actually want "provider X", he wanted the string from that entry for
    // editing purposes, and he's no longer looking at the popup to notice that,
    // despite deleting some text, the action we'll take on enter hasn't changed
    // at all.
    if (!had_keyword && (just_deleted_text_ || select_all_before_change_)) {
      popup_->manually_selected_match_.Clear();
    }
  }

  // Disable the fancy keyword UI if the user didn't already have a visible
  // keyword and is not at the end of the edit.  This prevents us from showing
  // the fancy UI (and interrupting the user's editing) if the user happens to
  // have a keyword for 'a', types 'ab' then puts a space between the 'a' and
  // the 'b'.
  disable_keyword_ui_ = (is_keyword_hint_ || keyword_.empty()) &&
      ((new_sel.cpMax != length) || (new_sel.cpMin != length));

  UpdatePopup();

  if (!had_keyword && !is_keyword_hint_ && !keyword_.empty()) {
    // Went from no selected keyword to a selected keyword. Set the affinity to
    // the keyword provider.  This forces the selected keyword to persist even
    // if the user deletes all the text.
    popup_->manually_selected_match_.Clear();
    popup_->manually_selected_match_.provider_affinity =
        popup_->autocomplete_controller()->keyword_provider();
  }

  if (text_differs)
    TextChanged();

  return true;
}

void AutocompleteEdit::SetUserText(const std::wstring& text,
                                   const std::wstring& display_text,
                                   bool update_popup) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  SetInputInProgress(true);
  paste_state_ = NONE;
  InternalSetUserText(text);
  SetWindowText(display_text.c_str());
  PlaceCaretAt(display_text.length());
  saved_selection_for_focus_change_.cpMin = -1;
  has_temporary_text_ = false;
  popup_->manually_selected_match_.Clear();
  if (update_popup)
    UpdatePopup();
  TextChanged();
}

void AutocompleteEdit::InternalSetUserText(const std::wstring& text) {
  user_text_ = text;
  just_deleted_text_ = false;
  inline_autocomplete_text_.clear();
}

void AutocompleteEdit::GetSelection(CHARRANGE& sel) const {
  GetSel(sel);

  // See if we need to reverse the direction of the selection.
  CComPtr<ITextSelection> selection;
  const HRESULT hr = GetTextObjectModel()->GetSelection(&selection);
  DCHECK(hr == S_OK);
  long flags;
  selection->GetFlags(&flags);
  if (flags & tomSelStartActive)
    std::swap(sel.cpMin, sel.cpMax);
}

std::wstring AutocompleteEdit::GetSelectedText() const {
  // Figure out the length of the selection.
  CHARRANGE sel;
  GetSel(sel);

  // Grab the selected text.
  std::wstring str;
  GetSelText(WriteInto(&str, sel.cpMax - sel.cpMin + 1));
  return str;
}

void AutocompleteEdit::SetSelection(LONG start, LONG end) {
  SetSel(start, end);

  if (start <= end)
    return;

  // We need to reverse the direction of the selection.
  CComPtr<ITextSelection> selection;
  const HRESULT hr = GetTextObjectModel()->GetSelection(&selection);
  DCHECK(hr == S_OK);
  selection->SetFlags(tomSelStartActive);
}

void AutocompleteEdit::PlaceCaretAt(std::wstring::size_type pos) {
  SetSelection(static_cast<LONG>(pos), static_cast<LONG>(pos));
}

bool AutocompleteEdit::IsSelectAll(const CHARRANGE& sel) const {
  const int text_length = GetTextLength();
  return ((sel.cpMin == 0) && (sel.cpMax >= text_length)) ||
      ((sel.cpMax == 0) && (sel.cpMin >= text_length));
}

LONG AutocompleteEdit::ClipXCoordToVisibleText(LONG x,
                                               bool is_triple_click) const {
  // Clip the X coordinate to the left edge of the text.  Careful:
  // PosFromChar(0) may return a negative X coordinate if the beginning of the
  // text has scrolled off the edit, so don't go past the clip rect's edge.
  RECT r;
  GetRect(&r);
  const int left_bound = std::max(r.left, PosFromChar(0).x);
  if (x < left_bound)
    return left_bound;

  // See if we need to clip to the right edge of the text.
  const int length = GetTextLength();
  // Asking for the coordinate of any character past the end of the text gets
  // the pixel just to the right of the last character.
  const int right_bound = std::min(r.right, PosFromChar(length).x);
  if ((length == 0) || (x < right_bound))
    return x;

  // For trailing characters that are 2 pixels wide of less (like "l" in some
  // fonts), we have a problem:
  //   * Clicks on any pixel within the character will place the cursor before
  //     the character.
  //   * Clicks on the pixel just after the character will not allow triple-
  //     click to work properly (true for any last character width).
  // So, we move to the last pixel of the character when this is a
  // triple-click, and moving to one past the last pixel in all other
  // scenarios.  This way, all clicks that can move the cursor will place it at
  // the end of the text, but triple-click will still work.
  return is_triple_click ? (right_bound - 1) : right_bound;
}

void AutocompleteEdit::EmphasizeURLComponents() {
  ITextDocument* const text_object_model = GetTextObjectModel();
  ScopedFreeze freeze(this, text_object_model);
  ScopedSuspendUndo suspend_undo(text_object_model);

  // Save the selection.
  CHARRANGE saved_sel;
  GetSelection(saved_sel);

  // See whether the contents are a URL with a non-empty host portion, which we
  // should emphasize.
  url_parse::Parsed parts;
  AutocompleteInput::Parse(GetText(), GetDesiredTLD(), &parts, NULL);
  bool emphasize = (parts.host.len > 0);
  // If !user_input_in_progress_, the permanent text is showing, which should
  // always be a URL, so no further checking is needed.  By avoiding checking in
  // this case, we avoid calling into the autocomplete providers, and thus
  // initializing the history system, as long as possible, which speeds startup.
  if (user_input_in_progress_) {
    // Rather than using the type returned by Parse(), key off the desired page
    // transition for this input since that can tell us whether an UNKNOWN input
    // string is going to be treated as a search or a navigation.  This is the
    // same method the Paste And Go system uses.
    PageTransition::Type transition = PageTransition::LINK;
    GetURLForCurrentText(&transition, NULL, NULL);
    if (transition != PageTransition::TYPED)
      emphasize = false;
  }

  // Set the baseline emphasis.
  CHARFORMAT cf = {0};
  cf.dwMask = CFM_COLOR;
  cf.dwEffects = 0;
  cf.crTextColor = (emphasize ? GetSysColor(COLOR_GRAYTEXT) :
                                GetSysColor(COLOR_WINDOWTEXT));
  SelectAll(false);
  SetSelectionCharFormat(cf);

  if (emphasize) {
    // We've found a host name, give it more emphasis.
    cf.crTextColor = GetSysColor(COLOR_WINDOWTEXT);
    SetSelection(parts.host.begin, parts.host.end());
    SetSelectionCharFormat(cf);
  }

  // Emphasize the scheme for security UI display purposes (if necessary).
  insecure_scheme_component_.reset();
  if (!user_input_in_progress_ && parts.scheme.is_nonempty() &&
      ((scheme_security_level_ == ToolbarModel::SECURE) ||
       (scheme_security_level_ == ToolbarModel::INSECURE))) {
    if (scheme_security_level_ == ToolbarModel::SECURE) {
      cf.crTextColor = kSecureSchemeColor;
    } else {
      insecure_scheme_component_.begin = parts.scheme.begin;
      insecure_scheme_component_.len = parts.scheme.len;
      cf.crTextColor = kInsecureSchemeColor;
    }
    SetSelection(parts.scheme.begin, parts.scheme.end());
    SetSelectionCharFormat(cf);
  }

  // Restore the selection.
  SetSelectionRange(saved_sel);
}

void AutocompleteEdit::EraseTopOfSelection(CDC* dc,
                                           const CRect& client_rect,
                                           const CRect& paint_clip_rect) {
  // Find the area we care about painting.   We could calculate the rect
  // containing just the selected portion, but there's no harm in simply erasing
  // the whole top of the client area, and at least once I saw us manage to
  // select the "phantom newline" briefly, which looks very weird if not clipped
  // off at the same height.
  CRect erase_rect(client_rect.left, client_rect.top, client_rect.right,
                   client_rect.top + font_y_adjustment_);
  erase_rect.IntersectRect(erase_rect, paint_clip_rect);

  // Erase to the background color.
  if (!erase_rect.IsRectNull())
    dc->FillSolidRect(&erase_rect, background_color_);
}

void AutocompleteEdit::DrawSlashForInsecureScheme(
    HDC hdc,
    const CRect& client_rect,
    const CRect& paint_clip_rect) {
  DCHECK(insecure_scheme_component_.is_nonempty());

  // Calculate the rect, in window coordinates, containing the portion of the
  // scheme where we'll be drawing the slash.  Vertically, we draw across one
  // x-height of text, plus an additional 3 stroke diameters (the stroke width
  // plus a half-stroke width of space between the stroke and the text, both
  // above and below the text).
  const int font_top = client_rect.top + font_y_adjustment_;
  const SkScalar kStrokeWidthPixels = SkIntToScalar(2);
  const int kAdditionalSpaceOutsideFont =
      static_cast<int>(ceil(kStrokeWidthPixels * 1.5f));
  const CRect scheme_rect(PosFromChar(insecure_scheme_component_.begin).x,
                          font_top + font_ascent_ - font_x_height_ -
                              kAdditionalSpaceOutsideFont,
                          PosFromChar(insecure_scheme_component_.end()).x,
                          font_top + font_ascent_ +
                              kAdditionalSpaceOutsideFont);

  // Clip to the portion we care about and translate to canvas coordinates
  // (see the canvas creation below) for use later.
  CRect canvas_clip_rect, canvas_paint_clip_rect;
  canvas_clip_rect.IntersectRect(scheme_rect, client_rect);
  canvas_paint_clip_rect.IntersectRect(canvas_clip_rect, paint_clip_rect);
  if (canvas_paint_clip_rect.IsRectNull())
    return;  // We don't need to paint any of this region, so just bail early.
  canvas_clip_rect.OffsetRect(-scheme_rect.left, -scheme_rect.top);
  canvas_paint_clip_rect.OffsetRect(-scheme_rect.left, -scheme_rect.top);

  // Create a paint context for drawing the antialiased stroke.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStrokeWidth(kStrokeWidthPixels);
  paint.setStrokeCap(SkPaint::kRound_Cap);

  // Create a canvas as large as |scheme_rect| to do our drawing, and initialize
  // it to fully transparent so any antialiasing will look nice when painted
  // atop the edit.
  ChromeCanvas canvas(scheme_rect.Width(), scheme_rect.Height(), false);
  // TODO (jcampan): This const_cast should not be necessary once the SKIA
  // API has been changed to return a non-const bitmap.
  (const_cast<SkBitmap&>(canvas.getDevice()->accessBitmap(true))).
      eraseARGB(0, 0, 0, 0);

  // Calculate the start and end of the stroke, which are just the lower left
  // and upper right corners of the canvas, inset by the radius of the endcap
  // so we don't clip the endcap off.
  const SkScalar kEndCapRadiusPixels = kStrokeWidthPixels / SkIntToScalar(2);
  const SkPoint start_point = {
      kEndCapRadiusPixels,
      SkIntToScalar(scheme_rect.Height()) - kEndCapRadiusPixels };
  const SkPoint end_point = {
      SkIntToScalar(scheme_rect.Width()) - kEndCapRadiusPixels,
      kEndCapRadiusPixels };

  // Calculate the selection rectangle in canvas coordinates, which we'll use
  // to clip the stroke so we can draw the unselected and selected portions.
  CHARRANGE sel;
  GetSel(sel);
  const SkRect selection_rect = {
      SkIntToScalar(PosFromChar(sel.cpMin).x - scheme_rect.left),
      SkIntToScalar(0),
      SkIntToScalar(PosFromChar(sel.cpMax).x - scheme_rect.left),
      SkIntToScalar(scheme_rect.Height()) };

  // Draw the unselected portion of the stroke.
  canvas.save();
  if (selection_rect.isEmpty() ||
      canvas.clipRect(selection_rect, SkRegion::kDifference_Op)) {
    paint.setColor(kSchemeStrikeoutColor);
    canvas.drawLine(start_point.fX, start_point.fY,
                    end_point.fX, end_point.fY, paint);
  }
  canvas.restore();

  // Draw the selected portion of the stroke.
  if (!selection_rect.isEmpty() && canvas.clipRect(selection_rect)) {
    paint.setColor(kSchemeSelectedStrikeoutColor);
    canvas.drawLine(start_point.fX, start_point.fY,
                    end_point.fX, end_point.fY, paint);
  }

  // Now copy what we drew to the target HDC.
  canvas.getTopPlatformDevice().drawToHDC(hdc,
      scheme_rect.left + canvas_paint_clip_rect.left - canvas_clip_rect.left,
      std::max(scheme_rect.top, client_rect.top) + canvas_paint_clip_rect.top -
          canvas_clip_rect.top, &canvas_paint_clip_rect);
}

void AutocompleteEdit::DrawDropHighlight(HDC hdc,
                                         const CRect& client_rect,
                                         const CRect& paint_clip_rect) {
  DCHECK(drop_highlight_position_ != -1);

  const int highlight_y = client_rect.top + font_y_adjustment_;
  const int highlight_x = PosFromChar(drop_highlight_position_).x - 1;
  const CRect highlight_rect(highlight_x,
                             highlight_y,
                             highlight_x + 1,
                             highlight_y + font_ascent_ + font_descent_);

  // Clip the highlight to the region being painted.
  CRect clip_rect;
  clip_rect.IntersectRect(highlight_rect, paint_clip_rect);
  if (clip_rect.IsRectNull())
    return;

  HGDIOBJ last_pen = SelectObject(hdc, CreatePen(PS_SOLID, 1, RGB(0, 0, 0)));
  MoveToEx(hdc, clip_rect.left, clip_rect.top, NULL);
  LineTo(hdc, clip_rect.left, clip_rect.bottom);
  DeleteObject(SelectObject(hdc, last_pen));
}

std::wstring AutocompleteEdit::GetDesiredTLD() const {
  return (control_key_state_ == DOWN_WITHOUT_CHANGE) ? L"com" : L"";
}

void AutocompleteEdit::UpdatePopup() {
  ScopedFreeze freeze(this, GetTextObjectModel());
  SetInputInProgress(true);

  if (!has_focus_) {
    // When we're in the midst of losing focus, don't rerun autocomplete.  This
    // can happen when losing focus causes the IME to cancel/finalize a
    // composition.  We still want to note that user input is in progress, we
    // just don't want to do anything else.
    //
    // Note that in this case the ScopedFreeze above was unnecessary; however,
    // we're inside the callstack of OnKillFocus(), which has already frozen the
    // edit, so this will never result in an unnecessary UpdateWindow() call.
    return;
  }

  // Figure out whether the user is trying to compose something in an IME.
  bool ime_composing = false;
  HIMC context = ImmGetContext(m_hWnd);
  if (context) {
    ime_composing = !!ImmGetCompositionString(context, GCS_COMPSTR, NULL, 0);
    ImmReleaseContext(m_hWnd, context);
  }

  // Don't inline autocomplete when:
  //   * The user is deleting text
  //   * The caret/selection isn't at the end of the text
  //   * The user has just pasted in something that replaced all the text
  //   * The user is trying to compose something in an IME
  CHARRANGE sel;
  GetSel(sel);
  popup_->StartAutocomplete(user_text_, GetDesiredTLD(),
      just_deleted_text_ || (sel.cpMax < GetTextLength()) ||
      (paste_state_ != NONE) || ime_composing);
}

void AutocompleteEdit::TextChanged() {
  ScopedFreeze freeze(this, GetTextObjectModel());
  EmphasizeURLComponents();
  controller_->OnChanged();
}

std::wstring AutocompleteEdit::DisplayTextFromUserText(
    const std::wstring& text) const {
  return (is_keyword_hint_ || keyword_.empty()) ?
      text : KeywordProvider::SplitReplacementStringFromInput(text);
}

std::wstring AutocompleteEdit::UserTextFromDisplayText(
    const std::wstring& text) const {
  return (is_keyword_hint_ || keyword_.empty()) ?
      text : (keyword_ + L" " + text);
}

std::wstring AutocompleteEdit::GetClipboardText() const {
  // Try text format.
  ClipboardService* clipboard = g_browser_process->clipboard_service();
  if (clipboard->IsFormatAvailable(CF_UNICODETEXT)) {
    std::wstring text;
    clipboard->ReadText(&text);

    // Note: Unlike in the find popup and textfield view, here we completely
    // remove whitespace strings containing newlines.  We assume users are
    // most likely pasting in URLs that may have been split into multiple
    // lines in terminals, email programs, etc., and so linebreaks indicate
    // completely bogus whitespace that would just cause the input to be
    // invalid.
    return CollapseWhitespace(text, true);
  }

  // Try bookmark format.
  //
  // It is tempting to try bookmark format first, but the URL we get out of a
  // bookmark has been cannonicalized via GURL.  This means if a user copies
  // and pastes from the URL bar to itself, the text will get fixed up and
  // cannonicalized, which is not what the user expects.  By pasting in this
  // order, we are sure to paste what the user copied.
  if (clipboard->IsFormatAvailable(ClipboardUtil::GetUrlWFormat()->cfFormat)) {
    std::string url_str;
    clipboard->ReadBookmark(NULL, &url_str);
    // pass resulting url string through GURL to normalize
    GURL url(url_str);
    if (url.is_valid())
      return UTF8ToWide(url.spec());
  }

  return std::wstring();
}

bool AutocompleteEdit::CanPasteAndGo(const std::wstring& text) const {
  if (popup_window_mode_)
    return false;

  // Reset local state.
  paste_and_go_url_.clear();
  paste_and_go_transition_ = PageTransition::TYPED;
  paste_and_go_alternate_nav_url_.clear();

  // See if the clipboard text can be parsed.
  const AutocompleteInput input(text, std::wstring(), true);
  if (input.type() == AutocompleteInput::INVALID)
    return false;

  // Ask the controller what do do with this input.
  paste_and_go_controller->SetProfile(profile_);
                              // This is cheap, and since there's one
                              // paste_and_go_controller for many tabs which
                              // may all have different profiles, it ensures
                              // we're always using the right one.
  const bool done = paste_and_go_controller->Start(input, false, true);
  DCHECK(done);
  AutocompleteResult result;
  paste_and_go_controller->GetResult(&result);
  if (result.empty())
    return false;

  // Set local state based on the default action for this input.
  result.SetDefaultMatch(AutocompleteResult::Selection());
  const AutocompleteResult::const_iterator match(result.default_match());
  DCHECK(match != result.end());
  paste_and_go_url_ = match->destination_url;
  paste_and_go_transition_ = match->transition;
  paste_and_go_alternate_nav_url_ = result.GetAlternateNavURL(input, match);

  return !paste_and_go_url_.empty();
}

void AutocompleteEdit::PasteAndGo() {
  // The final parameter to OpenURL, keyword, is not quite correct here: it's
  // possible to "paste and go" a string that contains a keyword.  This is
  // enough of an edge case that we ignore this possibility.
  RevertAll();
  OpenURL(paste_and_go_url_, CURRENT_TAB, paste_and_go_transition_,
          paste_and_go_alternate_nav_url_, AutocompletePopup::kNoMatch,
          std::wstring());
}

void AutocompleteEdit::SetInputInProgress(bool in_progress) {
  if (user_input_in_progress_ == in_progress)
    return;

  user_input_in_progress_ = in_progress;
  controller_->OnInputInProgress(in_progress);
}

void AutocompleteEdit::SendOpenNotification(size_t selected_line,
                                            const std::wstring& keyword) {
  // We only care about cases where there is a selection (i.e. the popup is
  // open).
  if (popup_->is_open()) {
    scoped_ptr<AutocompleteLog> log(popup_->GetAutocompleteLog());
    if (selected_line != AutocompletePopup::kNoMatch)
      log->selected_index = selected_line;
    else if (!has_temporary_text_)
      log->inline_autocompleted_length = inline_autocomplete_text_.length();
    NotificationService::current()->Notify(
        NOTIFY_OMNIBOX_OPENED_URL, Source<Profile>(profile_),
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

ITextDocument* AutocompleteEdit::GetTextObjectModel() const {
  if (!text_object_model_) {
    // This is lazily initialized, instead of being initialized in the
    // constructor, in order to avoid hurting startup performance.
    CComPtr<IRichEditOle> ole_interface;
    ole_interface.Attach(GetOleInterface());
    text_object_model_ = ole_interface;
  }
  return text_object_model_;
}

void AutocompleteEdit::StartDragIfNecessary(const CPoint& point) {
  if (initiated_drag_ || !win_util::IsDrag(mouse_down_point_, point))
    return;

  scoped_refptr<OSExchangeData> data = new OSExchangeData;

  DWORD supported_modes = DROPEFFECT_COPY;

  CHARRANGE sel;
  GetSelection(sel);

  // We're about to start a drag session, but the edit is expecting a mouse up
  // that it uses to reset internal state.  If we don't send a mouse up now,
  // when the mouse moves back into the edit the edit will reset the selection.
  // So, we send the event now which resets the selection.  We then restore the
  // selection and start the drag.  We always send lbuttonup as otherwise we
  // might trigger a context menu (right up).  This seems scary, but doesn't
  // seem to cause problems.
  {
    ScopedFreeze freeze(this, GetTextObjectModel());
    DefWindowProc(WM_LBUTTONUP, 0,
                  MAKELPARAM(mouse_down_point_.x, mouse_down_point_.y));
    SetSelectionRange(sel);
  }

  const std::wstring start_text(GetText());
  if (IsSelectAll(sel)) {
    // All the text is selected, export as URL.
    const std::wstring url(GetURLForCurrentText(NULL, NULL, NULL));
    std::wstring title;
    SkBitmap favicon;
    if (url == permanent_text_) {
      title = controller_->GetTitle();
      favicon = controller_->GetFavIcon();
    }
    const GURL gurl(url);
    drag_utils::SetURLAndDragImage(gurl, title, favicon, data.get());
    data->SetURL(gurl, title);
    supported_modes |= DROPEFFECT_LINK;
    UserMetrics::RecordAction(L"Omnibox_DragURL", profile_);
  } else {
    supported_modes |= DROPEFFECT_MOVE;
    UserMetrics::RecordAction(L"Omnibox_DragString", profile_);
  }

  data->SetString(GetSelectedText());

  scoped_refptr<BaseDragSource> drag_source(new BaseDragSource);
  DWORD dropped_mode;
  in_drag_ = true;
  if (DoDragDrop(data, drag_source, supported_modes, &dropped_mode) ==
      DRAGDROP_S_DROP) {
    if ((dropped_mode == DROPEFFECT_MOVE) && (start_text == GetText())) {
      ScopedFreeze freeze(this, GetTextObjectModel());
      OnBeforePossibleChange();
      SetSelectionRange(sel);
      ReplaceSel(L"", true);
      OnAfterPossibleChange();
    }
    // else case, not a move or it was a move and the drop was on us.
    // If the drop was on us, EditDropTarget took care of the move so that
    // we don't have to delete the text.
    possible_drag_ = false;
  } else {
    // Drag was canceled or failed. The mouse may still be down and
    // over us, in which case we need possible_drag_ to remain true so
    // that we don't forward mouse move events to the edit which will
    // start another drag.
    //
    // NOTE: we didn't use mouse capture during the mouse down as DoDragDrop
    // does its own capture.
    CPoint cursor_location;
    GetCursorPos(&cursor_location);

    CRect client_rect;
    GetClientRect(&client_rect);

    CPoint client_origin_on_screen(client_rect.left, client_rect.top);
    ClientToScreen(&client_origin_on_screen);
    client_rect.MoveToXY(client_origin_on_screen.x,
                         client_origin_on_screen.y);
    possible_drag_ = (client_rect.PtInRect(cursor_location) &&
                      ((GetKeyState(VK_LBUTTON) != 0) ||
                       (GetKeyState(VK_MBUTTON) != 0) ||
                       (GetKeyState(VK_RBUTTON) != 0)));
  }

  in_drag_ = false;
  initiated_drag_ = true;
  tracking_click_ = false;
}

void AutocompleteEdit::OnPossibleDrag(const CPoint& point) {
  if (possible_drag_)
    return;

  mouse_down_point_ = point;
  initiated_drag_ = false;

  CHARRANGE selection;
  GetSel(selection);
  if (selection.cpMin != selection.cpMax) {
    const POINT min_sel_location(PosFromChar(selection.cpMin));
    const POINT max_sel_location(PosFromChar(selection.cpMax));
    // NOTE: we don't consider the y location here as we always pass a
    // y-coordinate in the middle to the default handler which always triggers
    // a drag regardless of the y-coordinate.
    possible_drag_ = (point.x >= min_sel_location.x) &&
                     (point.x < max_sel_location.x);
  }
}

void AutocompleteEdit::UpdateDragDone(UINT keys) {
  possible_drag_ = (possible_drag_ &&
                    ((keys & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) != 0));
}

void AutocompleteEdit::RepaintDropHighlight(int position) {
  if ((position != -1) && (position <= GetTextLength())) {
    const POINT min_loc(PosFromChar(position));
    const RECT highlight_bounds = {min_loc.x - 1, font_y_adjustment_,
        min_loc.x + 2, font_ascent_ + font_descent_ + font_y_adjustment_};
    InvalidateRect(&highlight_bounds, false);
  }
}
