// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_edit_view_win.h"

#include <locale>

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/l10n_util_win.h"
#include "app/os_exchange_data.h"
#include "app/win_util.h"
#include "base/base_drag_source.h"
#include "base/base_drop_target.h"
#include "base/basictypes.h"
#include "base/clipboard.h"
#include "base/iat_patch.h"
#include "base/lazy_instance.h"
#include "base/ref_counted.h"
#include "base/scoped_clipboard_writer.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/autocomplete/autocomplete_accessibility.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view_win.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/common/gfx/utils.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/url_util.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_win.h"
#include "views/drag_utils.h"
#include "views/focus/focus_util_win.h"
#include "views/widget/widget.h"

#pragma comment(lib, "oleacc.lib")  // Needed for accessibility support.

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

namespace {

// EditDropTarget is the IDropTarget implementation installed on
// AutocompleteEditViewWin. EditDropTarget prefers URL over plain text. A drop
// of a URL replaces all the text of the edit and navigates immediately to the
// URL. A drop of plain text from the same edit either copies or moves the
// selected text, and a drop of plain text from a source other than the edit
// does a paste and go.
class EditDropTarget : public BaseDropTarget {
 public:
  explicit EditDropTarget(AutocompleteEditViewWin* edit);

 protected:
  virtual DWORD OnDragEnter(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect);
  virtual DWORD OnDragOver(IDataObject* data_object,
                           DWORD key_state,
                           POINT cursor_position,
                           DWORD effect);
  virtual void OnDragLeave(IDataObject* data_object);
  virtual DWORD OnDrop(IDataObject* data_object,
                       DWORD key_state,
                       POINT cursor_position,
                       DWORD effect);

 private:
  // If dragging a string, the drop highlight position of the edit is reset
  // based on the mouse position.
  void UpdateDropHighlightPosition(const POINT& cursor_screen_position);

  // Resets the visual drop indicates we install on the edit.
  void ResetDropHighlights();

  // The edit we're the drop target for.
  AutocompleteEditViewWin* edit_;

  // If true, the drag session contains a URL.
  bool drag_has_url_;

  // If true, the drag session contains a string. If drag_has_url_ is true,
  // this is false regardless of whether the clipboard has a string.
  bool drag_has_string_;

  DISALLOW_COPY_AND_ASSIGN(EditDropTarget);
};

// A helper method for determining a valid DROPEFFECT given the allowed
// DROPEFFECTS.  We prefer copy over link.
DWORD CopyOrLinkDropEffect(DWORD effect) {
  if (effect & DROPEFFECT_COPY)
    return DROPEFFECT_COPY;
  if (effect & DROPEFFECT_LINK)
    return DROPEFFECT_LINK;
  return DROPEFFECT_NONE;
}

EditDropTarget::EditDropTarget(AutocompleteEditViewWin* edit)
    : BaseDropTarget(edit->m_hWnd),
      edit_(edit),
      drag_has_url_(false),
      drag_has_string_(false) {
}

DWORD EditDropTarget::OnDragEnter(IDataObject* data_object,
                                  DWORD key_state,
                                  POINT cursor_position,
                                  DWORD effect) {
  OSExchangeData os_data(data_object);
  drag_has_url_ = os_data.HasURL();
  drag_has_string_ = !drag_has_url_ && os_data.HasString();
  if (drag_has_url_) {
    if (edit_->in_drag()) {
      // The edit we're associated with originated the drag. No point in
      // allowing the user to drop back on us.
      drag_has_url_ = false;
    }
    // NOTE: it would be nice to visually show all the text is going to
    // be replaced by selecting all, but this caused painting problems. In
    // particular the flashing caret would appear outside the edit! For now
    // we stick with no visual indicator other than that shown own the mouse
    // cursor.
  }
  return OnDragOver(data_object, key_state, cursor_position, effect);
}

DWORD EditDropTarget::OnDragOver(IDataObject* data_object,
                                 DWORD key_state,
                                 POINT cursor_position,
                                 DWORD effect) {
  if (drag_has_url_)
    return CopyOrLinkDropEffect(effect);

  if (drag_has_string_) {
    UpdateDropHighlightPosition(cursor_position);
    if (edit_->drop_highlight_position() == -1 && edit_->in_drag())
      return DROPEFFECT_NONE;
    if (edit_->in_drag()) {
      // The edit we're associated with originated the drag.  Do the normal drag
      // behavior.
      DCHECK((effect & DROPEFFECT_COPY) && (effect & DROPEFFECT_MOVE));
      return (key_state & MK_CONTROL) ? DROPEFFECT_COPY : DROPEFFECT_MOVE;
    }
    // Our edit didn't originate the drag, only allow link or copy.
    return CopyOrLinkDropEffect(effect);
  }

  return DROPEFFECT_NONE;
}

void EditDropTarget::OnDragLeave(IDataObject* data_object) {
  ResetDropHighlights();
}

DWORD EditDropTarget::OnDrop(IDataObject* data_object,
                             DWORD key_state,
                             POINT cursor_position,
                             DWORD effect) {
  OSExchangeData os_data(data_object);

  if (drag_has_url_) {
    GURL url;
    std::wstring title;
    if (os_data.GetURLAndTitle(&url, &title)) {
      edit_->SetUserText(UTF8ToWide(url.spec()));
      edit_->model()->AcceptInput(CURRENT_TAB, true);
      return CopyOrLinkDropEffect(effect);
    }
  } else if (drag_has_string_) {
    int string_drop_position = edit_->drop_highlight_position();
    std::wstring text;
    if ((string_drop_position != -1 || !edit_->in_drag()) &&
        os_data.GetString(&text)) {
      DCHECK(string_drop_position == -1 ||
             ((string_drop_position >= 0) &&
              (string_drop_position <= edit_->GetTextLength())));
      const DWORD drop_operation =
          OnDragOver(data_object, key_state, cursor_position, effect);
      if (edit_->in_drag()) {
        if (drop_operation == DROPEFFECT_MOVE)
          edit_->MoveSelectedText(string_drop_position);
        else
          edit_->InsertText(string_drop_position, text);
      } else {
        edit_->PasteAndGo(CollapseWhitespace(text, true));
      }
      ResetDropHighlights();
      return drop_operation;
    }
  }

  ResetDropHighlights();

  return DROPEFFECT_NONE;
}

void EditDropTarget::UpdateDropHighlightPosition(
    const POINT& cursor_screen_position) {
  if (drag_has_string_) {
    POINT client_position = cursor_screen_position;
    ScreenToClient(edit_->m_hWnd, &client_position);
    int drop_position = edit_->CharFromPos(client_position);
    if (edit_->in_drag()) {
      // Our edit originated the drag, don't allow a drop if over the selected
      // region.
      LONG sel_start, sel_end;
      edit_->GetSel(sel_start, sel_end);
      if ((sel_start != sel_end) && (drop_position >= sel_start) &&
          (drop_position <= sel_end))
        drop_position = -1;
    } else {
      // A drop from a source other than the edit replaces all the text, so
      // we don't show the drop location. See comment in OnDragEnter as to why
      // we don't try and select all here.
      drop_position = -1;
    }
    edit_->SetDropHighlightPosition(drop_position);
  }
}

void EditDropTarget::ResetDropHighlights() {
  if (drag_has_string_)
    edit_->SetDropHighlightPosition(-1);
}

// The AutocompleteEditState struct contains enough information about the
// AutocompleteEditModel and AutocompleteEditViewWin to save/restore a user's
// typing, caret position, etc. across tab changes.  We explicitly don't
// preserve things like whether the popup was open as this might be weird.
struct AutocompleteEditState {
  AutocompleteEditState(const AutocompleteEditModel::State model_state,
                        const AutocompleteEditViewWin::State view_state)
      : model_state(model_state),
        view_state(view_state) {
  }

  const AutocompleteEditModel::State model_state;
  const AutocompleteEditViewWin::State view_state;
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// Helper classes

AutocompleteEditViewWin::ScopedFreeze::ScopedFreeze(
    AutocompleteEditViewWin* edit,
    ITextDocument* text_object_model)
    : edit_(edit),
      text_object_model_(text_object_model) {
  // Freeze the screen.
  if (text_object_model_) {
    long count;
    text_object_model_->Freeze(&count);
  }
}

AutocompleteEditViewWin::ScopedFreeze::~ScopedFreeze() {
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

AutocompleteEditViewWin::ScopedSuspendUndo::ScopedSuspendUndo(
    ITextDocument* text_object_model)
    : text_object_model_(text_object_model) {
  // Suspend Undo processing.
  if (text_object_model_)
    text_object_model_->Undo(tomSuspend, NULL);
}

AutocompleteEditViewWin::ScopedSuspendUndo::~ScopedSuspendUndo() {
  // Resume Undo processing.
  if (text_object_model_)
    text_object_model_->Undo(tomResume, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// AutocompleteEditViewWin

// TODO (jcampan): these colors should be derived from the system colors to
// ensure they show properly. Bug #948807.
// Colors used to emphasize the scheme in the URL.

namespace {

const COLORREF kSecureSchemeColor = RGB(0, 150, 20);
const COLORREF kInsecureSchemeColor = RGB(200, 0, 0);

// Colors used to strike-out the scheme when it is insecure.
const SkColor kSchemeStrikeoutColor = SkColorSetRGB(210, 0, 0);
const SkColor kSchemeSelectedStrikeoutColor =
    SkColorSetRGB(255, 255, 255);

// These are used to hook the CRichEditCtrl's calls to BeginPaint() and
// EndPaint() and provide a memory DC instead.  See OnPaint().
HWND edit_hwnd = NULL;
PAINTSTRUCT paint_struct;

// Intercepted method for BeginPaint(). Must use __stdcall convention.
HDC WINAPI BeginPaintIntercept(HWND hWnd, LPPAINTSTRUCT lpPaint) {
  if (!edit_hwnd || (hWnd != edit_hwnd))
    return ::BeginPaint(hWnd, lpPaint);

  *lpPaint = paint_struct;
  return paint_struct.hdc;
}

// Intercepted method for EndPaint(). Must use __stdcall convention.
BOOL WINAPI EndPaintIntercept(HWND hWnd, const PAINTSTRUCT* lpPaint) {
  return (edit_hwnd && (hWnd == edit_hwnd)) ?
      true : ::EndPaint(hWnd, lpPaint);
}

// Returns a lazily initialized property bag accessor for saving our state in a
// TabContents.
PropertyAccessor<AutocompleteEditState>* GetStateAccessor() {
  static PropertyAccessor<AutocompleteEditState> state;
  return &state;
}

class PaintPatcher {
 public:
  PaintPatcher() : refcount_(0) { }
  ~PaintPatcher() { DCHECK(refcount_ == 0); }

  void RefPatch() {
    if (refcount_ == 0) {
      DCHECK(!begin_paint_.is_patched());
      DCHECK(!end_paint_.is_patched());
      begin_paint_.Patch(L"riched20.dll", "user32.dll", "BeginPaint",
                         &BeginPaintIntercept);
      end_paint_.Patch(L"riched20.dll", "user32.dll", "EndPaint",
                       &EndPaintIntercept);
    }
    ++refcount_;
  }

  void DerefPatch() {
    DCHECK(begin_paint_.is_patched());
    DCHECK(end_paint_.is_patched());
    --refcount_;
    if (refcount_ == 0) {
      begin_paint_.Unpatch();
      end_paint_.Unpatch();
    }
  }

 private:
  size_t refcount_;
  iat_patch::IATPatchFunction begin_paint_;
  iat_patch::IATPatchFunction end_paint_;

  DISALLOW_COPY_AND_ASSIGN(PaintPatcher);
};

base::LazyInstance<PaintPatcher> g_paint_patcher(base::LINKER_INITIALIZED);

}  // namespace

AutocompleteEditViewWin::AutocompleteEditViewWin(
    const gfx::Font& font,
    AutocompleteEditController* controller,
    ToolbarModel* toolbar_model,
    views::View* parent_view,
    HWND hwnd,
    Profile* profile,
    CommandUpdater* command_updater,
    bool popup_window_mode,
    AutocompletePopupPositioner* popup_positioner)
    : model_(new AutocompleteEditModel(this, controller, profile)),
      popup_view_(AutocompletePopupView::CreatePopupView(font, this,
                                                         model_.get(),
                                                         profile,
                                                         popup_positioner)),
      controller_(controller),
      parent_view_(parent_view),
      toolbar_model_(toolbar_model),
      command_updater_(command_updater),
      popup_window_mode_(popup_window_mode),
      force_hidden_(false),
      tracking_click_(false),
      tracking_double_click_(false),
      double_click_time_(0),
      can_discard_mousemove_(false),
      font_(font),
      possible_drag_(false),
      in_drag_(false),
      initiated_drag_(false),
      drop_highlight_position_(-1),
      background_color_(0),
      scheme_security_level_(ToolbarModel::NORMAL) {
  model_->set_popup_model(popup_view_->GetModel());

  saved_selection_for_focus_change_.cpMin = -1;

  g_paint_patcher.Pointer()->RefPatch();

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

AutocompleteEditViewWin::~AutocompleteEditViewWin() {
  NotificationService::current()->Notify(
      NotificationType::AUTOCOMPLETE_EDIT_DESTROYED,
      Source<AutocompleteEditViewWin>(this),
      NotificationService::NoDetails());

  // We balance our reference count and unpatch when the last instance has
  // been destroyed.  This prevents us from relying on the AtExit or static
  // destructor sequence to do our unpatching, which is generally fragile.
  g_paint_patcher.Pointer()->DerefPatch();
}

void AutocompleteEditViewWin::SaveStateToTab(TabContents* tab) {
  DCHECK(tab);

  const AutocompleteEditModel::State model_state(
      model_->GetStateForTabSwitch());

  CHARRANGE selection;
  GetSelection(selection);
  GetStateAccessor()->SetProperty(tab->property_bag(),
      AutocompleteEditState(
          model_state,
          State(selection, saved_selection_for_focus_change_)));
}

void AutocompleteEditViewWin::Update(
    const TabContents* tab_for_state_restoring) {
  const bool visibly_changed_permanent_text =
      model_->UpdatePermanentText(toolbar_model_->GetText());

  const ToolbarModel::SecurityLevel security_level =
      toolbar_model_->GetSchemeSecurityLevel();
  const COLORREF background_color = skia::SkColorToCOLORREF(
      LocationBarView::kBackgroundColorByLevel[security_level]);
  const bool changed_security_level =
      (security_level != scheme_security_level_);

  // Bail early when no visible state will actually change (prevents an
  // unnecessary ScopedFreeze, and thus UpdateWindow()).
  if ((background_color == background_color_) && !changed_security_level &&
      !visibly_changed_permanent_text && !tab_for_state_restoring)
    return;

  // Update our local state as desired.  We set scheme_security_level_ here so
  // it will already be correct before we get to any RevertAll()s below and use
  // it.
  ScopedFreeze freeze(this, GetTextObjectModel());
  if (background_color_ != background_color) {
    background_color_ = background_color;
    SetBackgroundColor(background_color_);
  }
  scheme_security_level_ = security_level;

  // When we're switching to a new tab, restore its state, if any.
  if (tab_for_state_restoring) {
    // Make sure we reset our own state first.  The new tab may not have any
    // saved state, or it may not have had input in progress, in which case we
    // won't overwrite all our local state.
    RevertAll();

    const AutocompleteEditState* state = GetStateAccessor()->GetProperty(
        tab_for_state_restoring->property_bag());
    if (state) {
      model_->RestoreState(state->model_state);

      // Restore user's selection.  We do this after restoring the user_text
      // above so we're selecting in the correct string.
      SetSelectionRange(state->view_state.selection);
      saved_selection_for_focus_change_ =
          state->view_state.saved_selection_for_focus_change;
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
    const bool was_sel_all = (sel.cpMin != sel.cpMax) &&
      IsSelectAllForRange(sel);

    RevertAll();

    if (was_sel_all)
      SelectAll(was_reversed);
  } else if (changed_security_level) {
    // Only the security style changed, nothing else.  Redraw our text using it.
    EmphasizeURLComponents();
  }
}

void AutocompleteEditViewWin::OpenURL(const GURL& url,
                                      WindowOpenDisposition disposition,
                                      PageTransition::Type transition,
                                      const GURL& alternate_nav_url,
                                      size_t selected_line,
                                      const std::wstring& keyword) {
  if (!url.is_valid())
    return;

  model_->SendOpenNotification(selected_line, keyword);

  ScopedFreeze freeze(this, GetTextObjectModel());
  if (disposition != NEW_BACKGROUND_TAB)
    RevertAll();  // Revert the box to its unedited state
  controller_->OnAutocompleteAccept(url, disposition, transition,
                                    alternate_nav_url);
}

std::wstring AutocompleteEditViewWin::GetText() const {
  const int len = GetTextLength() + 1;
  std::wstring str;
  GetWindowText(WriteInto(&str, len), len);
  return str;
}

void AutocompleteEditViewWin::SetUserText(const std::wstring& text,
                                          const std::wstring& display_text,
                                          bool update_popup) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  model_->SetUserText(text);
  saved_selection_for_focus_change_.cpMin = -1;
  SetWindowTextAndCaretPos(display_text, display_text.length());
  if (update_popup)
    UpdatePopup();
  TextChanged();
}

void AutocompleteEditViewWin::SetWindowTextAndCaretPos(const std::wstring& text,
                                                       size_t caret_pos) {
  HIMC imm_context = ImmGetContext(m_hWnd);
  if (imm_context) {
    // In Windows Vista, SetWindowText() automatically cancels any ongoing
    // IME composition, and updates the text of the underlying edit control.
    // In Windows XP, however, SetWindowText() gets applied to the IME
    // composition string if it exists, and doesn't update the underlying edit
    // control. To avoid this, we force the IME to cancel any outstanding
    // compositions here.  This is harmless in Vista and in cases where the IME
    // isn't composing.
    ImmNotifyIME(imm_context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    ImmReleaseContext(m_hWnd, imm_context);
  }

  SetWindowText(text.c_str());
  PlaceCaretAt(caret_pos);
}

void AutocompleteEditViewWin::SetForcedQuery() {
  const std::wstring current_text(GetText());
  if (current_text.empty() || (current_text[0] != '?'))
    SetUserText(L"?");
  else
    SetSelection(current_text.length(), 1);
}

bool AutocompleteEditViewWin::IsSelectAll() {
  CHARRANGE selection;
  GetSel(selection);
  return IsSelectAllForRange(selection);
}

void AutocompleteEditViewWin::SelectAll(bool reversed) {
  if (reversed)
    SetSelection(GetTextLength(), 0);
  else
    SetSelection(0, GetTextLength());
}

void AutocompleteEditViewWin::RevertAll() {
  ScopedFreeze freeze(this, GetTextObjectModel());
  ClosePopup();
  model_->Revert();
  saved_selection_for_focus_change_.cpMin = -1;
  TextChanged();
}

void AutocompleteEditViewWin::UpdatePopup() {
  ScopedFreeze freeze(this, GetTextObjectModel());
  model_->SetInputInProgress(true);

  if (!model_->has_focus()) {
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
  model_->StartAutocomplete((sel.cpMax < GetTextLength()) || ime_composing);
}

void AutocompleteEditViewWin::ClosePopup() {
  popup_view_->GetModel()->StopAutocomplete();
}

IAccessible* AutocompleteEditViewWin::GetIAccessible() {
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

void AutocompleteEditViewWin::SetDropHighlightPosition(int position) {
  if (drop_highlight_position_ != position) {
    RepaintDropHighlight(drop_highlight_position_);
    drop_highlight_position_ = position;
    RepaintDropHighlight(drop_highlight_position_);
  }
}

void AutocompleteEditViewWin::MoveSelectedText(int new_position) {
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

void AutocompleteEditViewWin::InsertText(int position,
                                         const std::wstring& text) {
  DCHECK((position >= 0) && (position <= GetTextLength()));
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  SetSelection(position, position);
  ReplaceSel(text.c_str());
  OnAfterPossibleChange();
}

void AutocompleteEditViewWin::OnTemporaryTextMaybeChanged(
    const std::wstring& display_text,
    bool save_original_selection) {
  if (save_original_selection)
    GetSelection(original_selection_);

  // Set new text and cursor position.  Sometimes this does extra work (e.g.
  // when the new text and the old text are identical), but it's only called
  // when the user manually changes the selected line in the popup, so that's
  // not really a problem.  Also, even when the text hasn't changed we'd want to
  // update the caret, because if the user had the cursor in the middle of the
  // text and then arrowed to another entry with the same text, we'd still want
  // to move the caret.
  ScopedFreeze freeze(this, GetTextObjectModel());
  SetWindowTextAndCaretPos(display_text, display_text.length());
  TextChanged();
}

bool AutocompleteEditViewWin::OnInlineAutocompleteTextMaybeChanged(
    const std::wstring& display_text,
    size_t user_text_length) {
  // Update the text and selection.  Because this can be called repeatedly while
  // typing, we've careful not to freeze the edit unless we really need to.
  // Also, unlike in the temporary text case above, here we don't want to update
  // the caret/selection unless we have to, since this might make the user's
  // caret position change without warning during typing.
  if (display_text == GetText())
    return false;

  ScopedFreeze freeze(this, GetTextObjectModel());
  // NOTE: We don't need the IME composition hack in SetWindowTextAndCaretPos()
  // here, because UpdatePopup() disables inline autocomplete when a
  // composition is in progress, thus preventing us from reaching this code.
  SetWindowText(display_text.c_str());
  // Set a reversed selection to keep the caret in the same position, which
  // avoids scrolling the user's text.
  SetSelection(static_cast<LONG>(display_text.length()),
               static_cast<LONG>(user_text_length));
  TextChanged();
  return true;
}

void AutocompleteEditViewWin::OnRevertTemporaryText() {
  SetSelectionRange(original_selection_);
  TextChanged();
}

void AutocompleteEditViewWin::OnBeforePossibleChange() {
  // Record our state.
  text_before_change_ = GetText();
  GetSelection(sel_before_change_);
}

bool AutocompleteEditViewWin::OnAfterPossibleChange() {
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
  const bool at_end_of_edit =
      (new_sel.cpMin == length) && (new_sel.cpMax == length);

  // See if the text or selection have changed since OnBeforePossibleChange().
  const std::wstring new_text(GetText());
  const bool text_differs = (new_text != text_before_change_);

  // When the user has deleted text, we don't allow inline autocomplete.  Make
  // sure to not flag cases like selecting part of the text and then pasting
  // (or typing) the prefix of that selection.  (We detect these by making
  // sure the caret, which should be after any insertion, hasn't moved
  // forward of the old selection start.)
  const bool just_deleted_text =
      (text_before_change_.length() > new_text.length()) &&
      (new_sel.cpMin <= std::min(sel_before_change_.cpMin,
                                 sel_before_change_.cpMax));


  const bool something_changed = model_->OnAfterPossibleChange(new_text,
      selection_differs, text_differs, just_deleted_text, at_end_of_edit);

  if (something_changed && text_differs)
    TextChanged();

  return something_changed;
}

void AutocompleteEditViewWin::PasteAndGo(const std::wstring& text) {
  if (CanPasteAndGo(text))
    model_->PasteAndGo();
}

bool AutocompleteEditViewWin::SkipDefaultKeyEventProcessing(
    const views::KeyEvent& e) {
  int c = e.GetCharacter();
  // We don't process ALT + numpad digit as accelerators, they are used for
  // entering special characters.  We do translate alt-home.
  if (e.IsAltDown() && (c != VK_HOME) &&
      win_util::IsNumPadDigit(c, e.IsExtendedKey()))
    return true;

  // Skip accelerators for key combinations omnibox wants to crack. This list
  // should be synced with OnKeyDownOnlyWritable() (but for tab which is dealt
  // with above in LocationBarView::SkipDefaultKeyEventProcessing).
  //
  // We cannot return true for all keys because we still need to handle some
  // accelerators (e.g., F5 for reload the page should work even when the
  // Omnibox gets focused).
  switch (c) {
    case VK_ESCAPE: {
      ScopedFreeze freeze(this, GetTextObjectModel());
      return model_->OnEscapeKeyPressed();
    }

    case VK_RETURN:
      return true;

    case VK_UP:
    case VK_DOWN:
      return !e.IsAltDown();

    case VK_DELETE:
    case VK_INSERT:
      return !e.IsAltDown() && e.IsShiftDown() && !e.IsControlDown();

    case 'X':
    case 'V':
      return !e.IsAltDown() && e.IsControlDown();

    case VK_BACK:
    case 0xbb:  // We don't use VK_OEM_PLUS in case the macro isn't defined.
                // (e.g., we don't have this symbol in embeded environment).
      return true;

    default:
      return false;
  }
}

void AutocompleteEditViewWin::HandleExternalMsg(UINT msg,
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

bool AutocompleteEditViewWin::IsCommandIdChecked(int command_id) const {
  return false;
}

bool AutocompleteEditViewWin::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case IDS_UNDO:         return !!CanUndo();
    case IDC_CUT:          return !!CanCut();
    case IDC_COPY:         return !!CanCopy();
    case IDC_PASTE:        return !!CanPaste();
    case IDS_PASTE_AND_GO: return CanPasteAndGo(GetClipboardText());
    case IDS_SELECT_ALL:   return !!CanSelectAll();
    case IDS_EDIT_SEARCH_ENGINES:
      return command_updater_->IsCommandEnabled(IDC_EDIT_SEARCH_ENGINES);
    default:               NOTREACHED(); return false;
  }
}

bool AutocompleteEditViewWin::GetAcceleratorForCommandId(
    int command_id,
    views::Accelerator* accelerator) {
  return parent_view_->GetWidget()->GetAccelerator(command_id, accelerator);
}

bool AutocompleteEditViewWin::IsLabelForCommandIdDynamic(int command_id) const {
  // No need to change the default IDS_PASTE_AND_GO label unless this is a
  // search.
  return command_id == IDS_PASTE_AND_GO;
}

std::wstring AutocompleteEditViewWin::GetLabelForCommandId(
    int command_id) const {
  DCHECK(command_id == IDS_PASTE_AND_GO);
  return l10n_util::GetString(model_->is_paste_and_search() ?
      IDS_PASTE_AND_SEARCH : IDS_PASTE_AND_GO);
}

void AutocompleteEditViewWin::ExecuteCommand(int command_id) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  if (command_id == IDS_PASTE_AND_GO) {
    // This case is separate from the switch() below since we don't want to wrap
    // it in OnBefore/AfterPossibleChange() calls.
    model_->PasteAndGo();
    return;
  }

  OnBeforePossibleChange();
  switch (command_id) {
    case IDS_UNDO:
      Undo();
      break;

    case IDC_CUT:
      Cut();
      break;

    case IDC_COPY:
      Copy();
      break;

    case IDC_PASTE:
      Paste();
      break;

    case IDS_SELECT_ALL:
      SelectAll(false);
      break;

    case IDS_EDIT_SEARCH_ENGINES:
      command_updater_->ExecuteCommand(IDC_EDIT_SEARCH_ENGINES);
      break;

    default:
      NOTREACHED();
      break;
  }
  OnAfterPossibleChange();
}

// static
int CALLBACK AutocompleteEditViewWin::WordBreakProc(LPTSTR edit_text,
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
bool AutocompleteEditViewWin::SchemeEnd(LPTSTR edit_text,
                                        int current_pos,
                                        int length) {
  return (current_pos >= 0) &&
         ((length - current_pos) > 2) &&
         (edit_text[current_pos] == ':') &&
         (edit_text[current_pos + 1] == '/') &&
         (edit_text[current_pos + 2] == '/');
}

void AutocompleteEditViewWin::OnChar(TCHAR ch, UINT repeat_count, UINT flags) {
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

void AutocompleteEditViewWin::OnContextMenu(HWND window, const CPoint& point) {
  BuildContextMenu();
  if (point.x == -1 || point.y == -1) {
    POINT p;
    GetCaretPos(&p);
    MapWindowPoints(HWND_DESKTOP, &p, 1);
    context_menu_->RunContextMenuAt(gfx::Point(p));
  } else {
    context_menu_->RunContextMenuAt(gfx::Point(point));
  }
}

void AutocompleteEditViewWin::OnCopy() {
  const std::wstring text(GetSelectedText());
  if (text.empty())
    return;

  ScopedClipboardWriter scw(g_browser_process->clipboard());
  scw.WriteText(text);

  // Check if the user is copying the whole address bar.  If they are, we
  // assume they are trying to copy a URL and write this to the clipboard as a
  // hyperlink.
  if (static_cast<int>(text.length()) < GetTextLength())
    return;

  // The entire control is selected.  Let's see what the user typed.  We can't
  // use model_->CurrentTextIsURL() or model_->GetDataForURLExport() because
  // right now the user is probably holding down control to cause the copy,
  // which will screw up our calculation of the desired_tld.
  GURL url;
  if (model_->GetURLForText(text, &url))
    scw.WriteHyperlink(text, url.spec());
}

void AutocompleteEditViewWin::OnCut() {
  OnCopy();

  // This replace selection will have no effect (even on the undo stack) if the
  // current selection is empty.
  ReplaceSel(L"", true);
}

LRESULT AutocompleteEditViewWin::OnGetObject(UINT uMsg,
                                             WPARAM wparam,
                                             LPARAM lparam) {
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

LRESULT AutocompleteEditViewWin::OnImeComposition(UINT message,
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

LRESULT AutocompleteEditViewWin::OnImeNotify(UINT message,
                                             WPARAM wparam,
                                             LPARAM lparam) {
  if (wparam == IMN_SETOPENSTATUS) {
    // A user has activated (or deactivated) IMEs (but not started a
    // composition).
    // Some IMEs get confused when we accept keywords while they are composing
    // text. To prevent this situation, we accept keywords when an IME is
    // activated.
    HIMC imm_context = ImmGetContext(m_hWnd);
    if (imm_context) {
      if (ImmGetOpenStatus(imm_context) &&
          model_->is_keyword_hint() && !model_->keyword().empty()) {
        ScopedFreeze freeze(this, GetTextObjectModel());
        model_->AcceptKeyword();
      }
      ImmReleaseContext(m_hWnd, imm_context);
    }
  }
  return DefWindowProc(message, wparam, lparam);
}

void AutocompleteEditViewWin::OnKeyDown(TCHAR key,
                                        UINT repeat_count,
                                        UINT flags) {
  if (OnKeyDownAllModes(key, repeat_count, flags) || popup_window_mode_ ||
      OnKeyDownOnlyWritable(key, repeat_count, flags))
    return;

  // CRichEditCtrl changes its text on WM_KEYDOWN instead of WM_CHAR for many
  // different keys (backspace, ctrl-v, ...), so we call this in both cases.
  HandleKeystroke(GetCurrentMessage()->message, key, repeat_count, flags);
}

void AutocompleteEditViewWin::OnKeyUp(TCHAR key,
                                      UINT repeat_count,
                                      UINT flags) {
  if (key == VK_CONTROL)
    model_->OnControlKeyChanged(false);

  SetMsgHandled(false);
}

void AutocompleteEditViewWin::OnKillFocus(HWND focus_wnd) {
  if (m_hWnd == focus_wnd) {
    // Focus isn't actually leaving.
    SetMsgHandled(false);
    return;
  }

  // Close the popup.
  ClosePopup();

  // Save the user's existing selection to restore it later.
  GetSelection(saved_selection_for_focus_change_);

  // Tell the model to reset itself.
  model_->OnKillFocus();

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
  if (model_->show_search_hint() ||
      (model_->is_keyword_hint() && !model_->keyword().empty()))
    controller_->OnChanged();

  // Cancel any user selection and scroll the text back to the beginning of the
  // URL.  We have to do this after calling DefWindowProc() because otherwise
  // an in-progress IME composition will be completed at the new caret position,
  // resulting in the string jumping unexpectedly to the front of the edit.
  PlaceCaretAt(0);
}

void AutocompleteEditViewWin::OnLButtonDblClk(UINT keys, const CPoint& point) {
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

void AutocompleteEditViewWin::OnLButtonDown(UINT keys, const CPoint& point) {
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

void AutocompleteEditViewWin::OnLButtonUp(UINT keys, const CPoint& point) {
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

LRESULT AutocompleteEditViewWin::OnMouseActivate(HWND window,
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
  if (!model_->has_focus() && (mouse_message == WM_LBUTTONDOWN) &&
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

void AutocompleteEditViewWin::OnMouseMove(UINT keys, const CPoint& point) {
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

void AutocompleteEditViewWin::OnPaint(HDC bogus_hdc) {
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

void AutocompleteEditViewWin::OnNonLButtonDown(UINT keys,
                                               const CPoint& point) {
  // Interestingly, the edit doesn't seem to cancel triple clicking when the
  // x-buttons (which usually means "thumb buttons") are pressed, so we only
  // call this for M and R down.
  tracking_double_click_ = false;

  OnPossibleDrag(point);

  SetMsgHandled(false);
}

void AutocompleteEditViewWin::OnNonLButtonUp(UINT keys, const CPoint& point) {
  UpdateDragDone(keys);

  // Let default handler have a crack at this.
  SetMsgHandled(false);
}

void AutocompleteEditViewWin::OnPaste() {
  // Replace the selection if we have something to paste.
  const std::wstring text(GetClipboardText());
  if (!text.empty()) {
    // If this paste will be replacing all the text, record that, so we can do
    // different behaviors in such a case.
    if (IsSelectAll())
      model_->on_paste_replacing_all();
    // Force a Paste operation to trigger the text_changed code in
    // OnAfterPossibleChange(), even if identical contents are pasted into the
    // text box.
    text_before_change_.clear();
    ReplaceSel(text.c_str(), true);
  }
}

void AutocompleteEditViewWin::OnSetFocus(HWND focus_wnd) {
  views::FocusManager* focus_manager = parent_view_->GetFocusManager();
  if (focus_manager) {
    // Notify the FocusManager that the focused view is now the location bar
    // (our parent view).
    focus_manager->SetFocusedView(parent_view_);
  } else {
    NOTREACHED();
  }

  model_->OnSetFocus(GetKeyState(VK_CONTROL) < 0);

  // Notify controller if it needs to show hint UI of some kind.
  ScopedFreeze freeze(this, GetTextObjectModel());
  if (model_->show_search_hint() ||
      (model_->is_keyword_hint() && !model_->keyword().empty()))
    controller_->OnChanged();

  // Restore saved selection if available.
  if (saved_selection_for_focus_change_.cpMin != -1) {
    SetSelectionRange(saved_selection_for_focus_change_);
    saved_selection_for_focus_change_.cpMin = -1;
  }

  SetMsgHandled(false);
}

void AutocompleteEditViewWin::OnSysChar(TCHAR ch,
                                        UINT repeat_count,
                                        UINT flags) {
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

void AutocompleteEditViewWin::OnWindowPosChanging(WINDOWPOS* window_pos) {
  if (force_hidden_)
    window_pos->flags &= ~SWP_SHOWWINDOW;
  SetMsgHandled(true);
}

BOOL AutocompleteEditViewWin::OnMouseWheel(UINT flags,
                                           short delta,
                                           CPoint point) {
  // Forward the mouse-wheel message to the window under the mouse.
  if (!views::RerouteMouseWheel(m_hWnd, MAKEWPARAM(flags, delta),
                                MAKELPARAM(point.x, point.y)))
    SetMsgHandled(false);
  return 0;
}

void AutocompleteEditViewWin::HandleKeystroke(UINT message,
                                              TCHAR key,
                                              UINT repeat_count,
                                              UINT flags) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(message, key, MAKELPARAM(repeat_count, flags));

  // CRichEditCtrl automatically turns on IMF_AUTOKEYBOARD when the user
  // inputs an RTL character, making it difficult for the user to control
  // what language is set as they type. Force this off to make the edit's
  // behavior more stable.
  const int lang_options = SendMessage(EM_GETLANGOPTIONS, 0, 0);
  if (lang_options & IMF_AUTOKEYBOARD)
    SendMessage(EM_SETLANGOPTIONS, 0, lang_options & ~IMF_AUTOKEYBOARD);

  OnAfterPossibleChange();
}

bool AutocompleteEditViewWin::OnKeyDownOnlyWritable(TCHAR key,
                                                    UINT repeat_count,
                                                    UINT flags) {
  // NOTE: Annoyingly, ctrl-alt-<key> generates WM_KEYDOWN rather than
  // WM_SYSKEYDOWN, so we need to check (flags & KF_ALTDOWN) in various places
  // in this function even with a WM_SYSKEYDOWN handler.

  // Update LocationBarView::SkipDefaultKeyEventProcessing() as well when you
  // add some key combinations here.
  int count = repeat_count;
  switch (key) {
    case VK_RETURN:
      model_->AcceptInput((flags & KF_ALTDOWN) ?
          NEW_FOREGROUND_TAB : CURRENT_TAB, false);
      return true;

    case VK_PRIOR:
    case VK_NEXT:
      count = model_->result().size();
      // FALL THROUGH
    case VK_UP:
    case VK_DOWN:
      // Ignore alt + numpad, but treat alt + (non-numpad) like (non-numpad).
      if ((flags & KF_ALTDOWN) && !(flags & KF_EXTENDED))
        return false;

      model_->OnUpOrDownKeyPressed(((key == VK_PRIOR) || (key == VK_UP)) ?
          -count : count);
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
      if (GetKeyState(VK_CONTROL) >= 0) {
        // Cut text if possible.
        CHARRANGE selection;
        GetSel(selection);
        if (selection.cpMin != selection.cpMax) {
          ScopedFreeze freeze(this, GetTextObjectModel());
          OnBeforePossibleChange();
          Cut();
          OnAfterPossibleChange();
        } else {
          AutocompletePopupModel* popup_model = popup_view_->GetModel();
          if (popup_model->IsOpen()) {
            // This is a bit overloaded, but we hijack Shift-Delete in this
            // case to delete the current item from the pop-up.  We prefer
            // cutting to this when possible since that's the behavior more
            // people expect from Shift-Delete, and it's more commonly useful.
            popup_model->TryDeletingCurrentItem();
          }
        }
      }
      return true;

    case 'X':
      if ((flags & KF_ALTDOWN) || (GetKeyState(VK_CONTROL) >= 0))
        return false;
      if (GetKeyState(VK_SHIFT) >= 0) {
        ScopedFreeze freeze(this, GetTextObjectModel());
        OnBeforePossibleChange();
        Cut();
        OnAfterPossibleChange();
      }
      return true;

    case VK_INSERT:
      // Ignore insert by itself, so we don't turn overtype mode on/off.
      if (!(flags & KF_ALTDOWN) && (GetKeyState(VK_SHIFT) >= 0) &&
          (GetKeyState(VK_CONTROL) >= 0))
        return true;
      // FALL THROUGH
    case 'V':
      if ((flags & KF_ALTDOWN) ||
          (GetKeyState((key == 'V') ? VK_CONTROL : VK_SHIFT) >= 0))
        return false;
      if (GetKeyState((key == 'V') ? VK_SHIFT : VK_CONTROL) >= 0) {
        ScopedFreeze freeze(this, GetTextObjectModel());
        OnBeforePossibleChange();
        Paste();
        OnAfterPossibleChange();
      }
      return true;

    case VK_BACK: {
      if ((flags & KF_ALTDOWN) || model_->is_keyword_hint() ||
          model_->keyword().empty())
        return false;

      {
        CHARRANGE selection;
        GetSel(selection);
        if ((selection.cpMin != selection.cpMax) || (selection.cpMin != 0))
          return false;
      }

      // We're showing a keyword and the user pressed backspace at the beginning
      // of the text. Delete the selected keyword.
      ScopedFreeze freeze(this, GetTextObjectModel());
      model_->ClearKeyword(GetText());
      return true;
    }

    case VK_TAB: {
      if (model_->is_keyword_hint() && !model_->keyword().empty()) {
        // Accept the keyword.
        ScopedFreeze freeze(this, GetTextObjectModel());
        model_->AcceptKeyword();
      }
      return true;
    }

    case 0xbb:  // Ctrl-'='.  Triggers subscripting (even in plain text mode).
                // We don't use VK_OEM_PLUS in case the macro isn't defined.
                // (e.g., we don't have this symbol in embeded environment).
      return true;

    default:
      return false;
  }
}

bool AutocompleteEditViewWin::OnKeyDownAllModes(TCHAR key,
                                                UINT repeat_count,
                                                UINT flags) {
  // See KF_ALTDOWN comment atop OnKeyDownOnlyWriteable().

  switch (key) {
    case VK_CONTROL:
      model_->OnControlKeyChanged(true);
      return false;

    case 'C':
      // See more detailed comments in OnKeyDownOnlyWriteable().
      if ((flags & KF_ALTDOWN) || (GetKeyState(VK_CONTROL) >= 0))
        return false;
      if (GetKeyState(VK_SHIFT) >= 0)
        Copy();
      return true;

    default:
      return false;
  }
}

void AutocompleteEditViewWin::GetSelection(CHARRANGE& sel) const {
  GetSel(sel);

  // See if we need to reverse the direction of the selection.
  ITextDocument* const text_object_model = GetTextObjectModel();
  if (!text_object_model)
    return;
  CComPtr<ITextSelection> selection;
  const HRESULT hr = text_object_model->GetSelection(&selection);
  DCHECK(hr == S_OK);
  long flags;
  selection->GetFlags(&flags);
  if (flags & tomSelStartActive)
    std::swap(sel.cpMin, sel.cpMax);
}

std::wstring AutocompleteEditViewWin::GetSelectedText() const {
  // Figure out the length of the selection.
  CHARRANGE sel;
  GetSel(sel);

  // Grab the selected text.
  std::wstring str;
  GetSelText(WriteInto(&str, sel.cpMax - sel.cpMin + 1));
  return str;
}

void AutocompleteEditViewWin::SetSelection(LONG start, LONG end) {
  SetSel(start, end);

  if (start <= end)
    return;

  // We need to reverse the direction of the selection.
  ITextDocument* const text_object_model = GetTextObjectModel();
  if (!text_object_model)
    return;
  CComPtr<ITextSelection> selection;
  const HRESULT hr = text_object_model->GetSelection(&selection);
  DCHECK(hr == S_OK);
  selection->SetFlags(tomSelStartActive);
}

void AutocompleteEditViewWin::PlaceCaretAt(std::wstring::size_type pos) {
  SetSelection(static_cast<LONG>(pos), static_cast<LONG>(pos));
}

bool AutocompleteEditViewWin::IsSelectAllForRange(const CHARRANGE& sel) const {
  const int text_length = GetTextLength();
  return ((sel.cpMin == 0) && (sel.cpMax >= text_length)) ||
      ((sel.cpMax == 0) && (sel.cpMin >= text_length));
}

LONG AutocompleteEditViewWin::ClipXCoordToVisibleText(
    LONG x, bool is_triple_click) const {
  // Clip the X coordinate to the left edge of the text. Careful:
  // PosFromChar(0) may return a negative X coordinate if the beginning of the
  // text has scrolled off the edit, so don't go past the clip rect's edge.
  PARAFORMAT2 pf2;
  GetParaFormat(pf2);
  // Calculation of the clipped coordinate is more complicated if the paragraph
  // layout is RTL layout, or if there is RTL characters inside the LTR layout
  // paragraph.
  bool ltr_text_in_ltr_layout = true;
  if ((pf2.wEffects & PFE_RTLPARA) ||
      l10n_util::StringContainsStrongRTLChars(GetText())) {
    ltr_text_in_ltr_layout = false;
  }
  const int length = GetTextLength();
  RECT r;
  GetRect(&r);
  // The values returned by PosFromChar() seem to refer always
  // to the left edge of the character's bounding box.
  const LONG first_position_x = PosFromChar(0).x;
  LONG min_x = first_position_x;
  if (!ltr_text_in_ltr_layout) {
    for (int i = 1; i < length; ++i)
      min_x = std::min(min_x, PosFromChar(i).x);
  }
  const LONG left_bound = std::max(r.left, min_x);
  // PosFromChar(length) is a phantom character past the end of the text. It is
  // not necessarily a right bound; in RTL controls it may be a left bound. So
  // treat it as a right bound only if it is to the right of the first
  // character.
  LONG right_bound = r.right;
  LONG end_position_x = PosFromChar(length).x;
  if (end_position_x >= first_position_x) {
    right_bound = std::min(right_bound, end_position_x);  // LTR case.
  }
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
  if (x < left_bound) {
    return (is_triple_click && ltr_text_in_ltr_layout) ? left_bound - 1 :
                                                         left_bound;
  }
  if ((length == 0) || (x < right_bound))
    return x;
  return is_triple_click ? (right_bound - 1) : right_bound;
}

void AutocompleteEditViewWin::EmphasizeURLComponents() {
  ITextDocument* const text_object_model = GetTextObjectModel();
  ScopedFreeze freeze(this, text_object_model);
  ScopedSuspendUndo suspend_undo(text_object_model);

  // Save the selection.
  CHARRANGE saved_sel;
  GetSelection(saved_sel);

  // See whether the contents are a URL with a non-empty host portion, which we
  // should emphasize.  To check for a URL, rather than using the type returned
  // by Parse(), ask the model, which will check the desired page transition for
  // this input.  This can tell us whether an UNKNOWN input string is going to
  // be treated as a search or a navigation, and is the same method the Paste
  // And Go system uses.
  url_parse::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(
      GetText(), model_->GetDesiredTLD(), &scheme, &host);
  const bool emphasize = model_->CurrentTextIsURL() && (host.len > 0);

  // Set the baseline emphasis.
  CHARFORMAT cf = {0};
  cf.dwMask = CFM_COLOR;
  cf.dwEffects = 0;
  cf.crTextColor = GetSysColor(emphasize ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT);
  SelectAll(false);
  SetSelectionCharFormat(cf);

  if (emphasize) {
    // We've found a host name, give it more emphasis.
    cf.crTextColor = GetSysColor(COLOR_WINDOWTEXT);
    SetSelection(host.begin, host.end());
    SetSelectionCharFormat(cf);
  }

  // Emphasize the scheme for security UI display purposes (if necessary).
  insecure_scheme_component_.reset();
  if (!model_->user_input_in_progress() && scheme.is_nonempty() &&
      (scheme_security_level_ != ToolbarModel::NORMAL)) {
    if (scheme_security_level_ == ToolbarModel::SECURE) {
      cf.crTextColor = kSecureSchemeColor;
    } else {
      insecure_scheme_component_.begin = scheme.begin;
      insecure_scheme_component_.len = scheme.len;
      cf.crTextColor = kInsecureSchemeColor;
    }
    SetSelection(scheme.begin, scheme.end());
    SetSelectionCharFormat(cf);
  }

  // Restore the selection.
  SetSelectionRange(saved_sel);
}

void AutocompleteEditViewWin::EraseTopOfSelection(
    CDC* dc, const CRect& client_rect, const CRect& paint_clip_rect) {
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

void AutocompleteEditViewWin::DrawSlashForInsecureScheme(
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
  gfx::Canvas canvas(scheme_rect.Width(), scheme_rect.Height(), false);
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

void AutocompleteEditViewWin::DrawDropHighlight(
    HDC hdc, const CRect& client_rect, const CRect& paint_clip_rect) {
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

void AutocompleteEditViewWin::TextChanged() {
  ScopedFreeze freeze(this, GetTextObjectModel());
  EmphasizeURLComponents();
  controller_->OnChanged();
}

std::wstring AutocompleteEditViewWin::GetClipboardText() const {
  // Try text format.
  Clipboard* clipboard = g_browser_process->clipboard();
  if (clipboard->IsFormatAvailable(Clipboard::GetPlainTextWFormatType())) {
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
  if (clipboard->IsFormatAvailable(Clipboard::GetUrlWFormatType())) {
    std::string url_str;
    clipboard->ReadBookmark(NULL, &url_str);
    // pass resulting url string through GURL to normalize
    GURL url(url_str);
    if (url.is_valid())
      return UTF8ToWide(url.spec());
  }

  return std::wstring();
}

bool AutocompleteEditViewWin::CanPasteAndGo(const std::wstring& text) const {
  return !popup_window_mode_ && model_->CanPasteAndGo(text);
}

ITextDocument* AutocompleteEditViewWin::GetTextObjectModel() const {
  if (!text_object_model_) {
    // This is lazily initialized, instead of being initialized in the
    // constructor, in order to avoid hurting startup performance.
    CComPtr<IRichEditOle> ole_interface;
    ole_interface.Attach(GetOleInterface());
    text_object_model_ = ole_interface;
  }
  return text_object_model_;
}

void AutocompleteEditViewWin::StartDragIfNecessary(const CPoint& point) {
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
  if (IsSelectAllForRange(sel)) {
    // All the text is selected, export as URL.
    GURL url;
    std::wstring title;
    SkBitmap favicon;
    model_->GetDataForURLExport(&url, &title, &favicon);
    drag_utils::SetURLAndDragImage(url, title, favicon, data.get());
    data->SetURL(url, title);
    supported_modes |= DROPEFFECT_LINK;
    UserMetrics::RecordAction(L"Omnibox_DragURL", model_->profile());
  } else {
    supported_modes |= DROPEFFECT_MOVE;
    UserMetrics::RecordAction(L"Omnibox_DragString", model_->profile());
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

void AutocompleteEditViewWin::OnPossibleDrag(const CPoint& point) {
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

void AutocompleteEditViewWin::UpdateDragDone(UINT keys) {
  possible_drag_ = (possible_drag_ &&
                    ((keys & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) != 0));
}

void AutocompleteEditViewWin::RepaintDropHighlight(int position) {
  if ((position != -1) && (position <= GetTextLength())) {
    const POINT min_loc(PosFromChar(position));
    const RECT highlight_bounds = {min_loc.x - 1, font_y_adjustment_,
        min_loc.x + 2, font_ascent_ + font_descent_ + font_y_adjustment_};
    InvalidateRect(&highlight_bounds, false);
  }
}

void AutocompleteEditViewWin::BuildContextMenu() {
  if (context_menu_contents_.get())
    return;

  context_menu_contents_.reset(new views::SimpleMenuModel(this));
  // Set up context menu.
  if (popup_window_mode_) {
    context_menu_contents_->AddItemWithStringId(IDS_COPY, IDS_COPY);
  } else {
    context_menu_contents_->AddItemWithStringId(IDS_UNDO, IDS_UNDO);
    context_menu_contents_->AddSeparator();
    context_menu_contents_->AddItemWithStringId(IDC_CUT, IDS_CUT);
    context_menu_contents_->AddItemWithStringId(IDC_COPY, IDS_COPY);
    context_menu_contents_->AddItemWithStringId(IDC_PASTE, IDS_PASTE);
    // GetContextualLabel() will override this next label with the
    // IDS_PASTE_AND_SEARCH label as needed.
    context_menu_contents_->AddItemWithStringId(IDS_PASTE_AND_GO,
                                                IDS_PASTE_AND_GO);
    context_menu_contents_->AddSeparator();
    context_menu_contents_->AddItemWithStringId(IDS_SELECT_ALL, IDS_SELECT_ALL);
    context_menu_contents_->AddSeparator();
    context_menu_contents_->AddItemWithStringId(IDS_EDIT_SEARCH_ENGINES,
                                                IDS_EDIT_SEARCH_ENGINES);
  }
  context_menu_.reset(new views::Menu2(context_menu_contents_.get()));
}
