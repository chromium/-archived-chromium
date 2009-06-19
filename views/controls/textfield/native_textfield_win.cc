// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "app/l10n_util_win.h"
#include "app/win_util.h"
#include "base/clipboard.h"
#include "base/gfx/native_theme.h"
#include "base/scoped_clipboard_writer.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "grit/app_strings.h"
#include "skia/ext/skia_utils_win.h"
#include "views/controls/menu/menu_win.h"
#include "views/controls/native/native_view_host.h"
#include "views/controls/textfield/native_textfield_win.h"
#include "views/controls/textfield/textfield.h"
#include "views/focus/focus_manager.h"
#include "views/focus/focus_util_win.h"
#include "views/views_delegate.h"
#include "views/widget/widget.h"

namespace views {

///////////////////////////////////////////////////////////////////////////////
// Helper classes

NativeTextfieldWin::ScopedFreeze::ScopedFreeze(NativeTextfieldWin* edit,
                                               ITextDocument* text_object_model)
    : edit_(edit),
      text_object_model_(text_object_model) {
  // Freeze the screen.
  if (text_object_model_) {
    long count;
    text_object_model_->Freeze(&count);
  }
}

NativeTextfieldWin::ScopedFreeze::~ScopedFreeze() {
  // Unfreeze the screen.
  if (text_object_model_) {
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

///////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWin

bool NativeTextfieldWin::did_load_library_ = false;

NativeTextfieldWin::NativeTextfieldWin(Textfield* textfield)
    : textfield_(textfield),
      tracking_double_click_(false),
      double_click_time_(0),
      can_discard_mousemove_(false),
      contains_mouse_(false),
      ime_discard_composition_(false),
      ime_composition_start_(0),
      ime_composition_length_(0),
      bg_color_(0) {
  if (!did_load_library_)
    did_load_library_ = !!LoadLibrary(L"riched20.dll");

  DWORD style = kDefaultEditStyle;
  if (textfield_->style() & Textfield::STYLE_PASSWORD)
    style |= ES_PASSWORD;

  if (textfield_->read_only())
    style |= ES_READONLY;

  if (textfield_->style() & Textfield::STYLE_MULTILINE)
    style |= ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL;
  else
    style |= ES_AUTOHSCROLL;
  // Make sure we apply RTL related extended window styles if necessary.
  DWORD ex_style = l10n_util::GetExtendedStyles();

  RECT r = {0, 0, textfield_->width(), textfield_->height()};
  Create(textfield_->GetWidget()->GetNativeView(), r, NULL, style, ex_style);

  if (textfield_->style() & Textfield::STYLE_LOWERCASE) {
    DCHECK((textfield_->style() & Textfield::STYLE_PASSWORD) == 0);
    SetEditStyle(SES_LOWERCASE, SES_LOWERCASE);
  }

  // Set up the text_object_model_.
  CComPtr<IRichEditOle> ole_interface;
  ole_interface.Attach(GetOleInterface());
  text_object_model_ = ole_interface;

  container_view_ = new NativeViewHost;
  textfield_->AddChildView(container_view_);
  container_view_->set_focus_view(textfield_);
  container_view_->Attach(m_hWnd);
}

NativeTextfieldWin::~NativeTextfieldWin() {
  if (IsWindow())
    DestroyWindow();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWin, NativeTextfieldWrapper implementation:

std::wstring NativeTextfieldWin::GetText() const {
  int len = GetTextLength() + 1;
  std::wstring str;
  GetWindowText(WriteInto(&str, len), len);
  return str;
}

void NativeTextfieldWin::UpdateText() {
  std::wstring text = textfield_->text();
  // Adjusting the string direction before setting the text in order to make
  // sure both RTL and LTR strings are displayed properly.
  std::wstring text_to_set;
  if (!l10n_util::AdjustStringForLocaleDirection(text, &text_to_set))
    text_to_set = text;
  if (textfield_->style() & Textfield::STYLE_LOWERCASE)
    text_to_set = l10n_util::ToLower(text_to_set);
  SetWindowText(text_to_set.c_str());
}

void NativeTextfieldWin::AppendText(const std::wstring& text) {
  int text_length = GetWindowTextLength();
  ::SendMessage(m_hWnd, TBM_SETSEL, true, MAKELPARAM(text_length, text_length));
  ::SendMessage(m_hWnd, EM_REPLACESEL, false,
                reinterpret_cast<LPARAM>(text.c_str()));
}

std::wstring NativeTextfieldWin::GetSelectedText() const {
  // Figure out the length of the selection.
  long start;
  long end;
  GetSel(start, end);

  // Grab the selected text.
  std::wstring str;
  GetSelText(WriteInto(&str, end - start + 1));

  return str;
}

void NativeTextfieldWin::SelectAll() {
  // Select from the end to the front so that the first part of the text is
  // always visible.
  SetSel(GetTextLength(), 0);
}

void NativeTextfieldWin::ClearSelection() {
  SetSel(GetTextLength(), GetTextLength());
}

void NativeTextfieldWin::UpdateBorder() {
  SetWindowPos(NULL, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_FRAMECHANGED | SWP_NOACTIVATE |
               SWP_NOOWNERZORDER | SWP_NOSIZE);
}

void NativeTextfieldWin::UpdateBackgroundColor() {
  if (!textfield_->use_default_background_color()) {
    bg_color_ = skia::SkColorToCOLORREF(textfield_->background_color());
  } else {
    bg_color_ = GetSysColor(textfield_->read_only() ? COLOR_3DFACE
                                                    : COLOR_WINDOW);
  }
  CRichEditCtrl::SetBackgroundColor(bg_color_);
}

void NativeTextfieldWin::UpdateReadOnly() {
  SendMessage(m_hWnd, EM_SETREADONLY, textfield_->read_only(), 0);
}

void NativeTextfieldWin::UpdateFont() {
  SendMessage(m_hWnd, WM_SETFONT,
              reinterpret_cast<WPARAM>(textfield_->font().hfont()), TRUE);
}

void NativeTextfieldWin::UpdateEnabled() {
  SendMessage(m_hWnd, WM_ENABLE, textfield_->IsEnabled(), 0);
}

void NativeTextfieldWin::SetHorizontalMargins(int left, int right) {
  // SendMessage expects the two values to be packed into one using MAKELONG
  // so we truncate to 16 bits if necessary.
  SendMessage(m_hWnd, EM_SETMARGINS,
              EC_LEFTMARGIN | EC_RIGHTMARGIN,
              MAKELONG(left  & 0xFFFF, right & 0xFFFF));
}

void NativeTextfieldWin::SetFocus() {
  // Focus the associated HWND.
  //container_view_->Focus();
  ::SetFocus(m_hWnd);
}

View* NativeTextfieldWin::GetView() {
  return container_view_;
}

gfx::NativeView NativeTextfieldWin::GetTestingHandle() const {
  return m_hWnd;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWin, SimpleMenuModel::Delegate implementation:

bool NativeTextfieldWin::IsCommandIdChecked(int command_id) const {
  return false;
}

bool NativeTextfieldWin::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case IDS_APP_UNDO:       return !textfield_->read_only() && !!CanUndo();
    case IDS_APP_CUT:        return !textfield_->read_only() &&
                                    !textfield_->IsPassword() && !!CanCut();
    case IDS_APP_COPY:       return !!CanCopy() && !textfield_->IsPassword();
    case IDS_APP_PASTE:      return !textfield_->read_only() && !!CanPaste();
    case IDS_APP_SELECT_ALL: return !!CanSelectAll();
    default:                 NOTREACHED();
                             return false;
  }
}

bool NativeTextfieldWin::GetAcceleratorForCommandId(int command_id,
                                                    Accelerator* accelerator) {
  // The standard Ctrl-X, Ctrl-V and Ctrl-C are not defined as accelerators
  // anywhere so we need to check for them explicitly here.
  switch (command_id) {
    case IDS_APP_CUT:
      *accelerator = views::Accelerator(L'X', false, true, false);
      return true;
    case IDS_APP_COPY:
      *accelerator = views::Accelerator(L'C', false, true, false);
      return true;
    case IDS_APP_PASTE:
      *accelerator = views::Accelerator(L'V', false, true, false);
      return true;
  }
  return container_view_->GetWidget()->GetAccelerator(command_id, accelerator);
}

void NativeTextfieldWin::ExecuteCommand(int command_id) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  switch (command_id) {
    case IDS_APP_UNDO:       Undo();       break;
    case IDS_APP_CUT:        Cut();        break;
    case IDS_APP_COPY:       Copy();       break;
    case IDS_APP_PASTE:      Paste();      break;
    case IDS_APP_SELECT_ALL: SelectAll();  break;
    default:                 NOTREACHED(); break;
  }
  OnAfterPossibleChange();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWin, private:

void NativeTextfieldWin::OnChar(TCHAR ch, UINT repeat_count, UINT flags) {
  HandleKeystroke(GetCurrentMessage()->message, ch, repeat_count, flags);
}

void NativeTextfieldWin::OnContextMenu(HWND window, const CPoint& point) {
  CPoint p(point);
  if (point.x == -1 || point.y == -1) {
    GetCaretPos(&p);
    MapWindowPoints(HWND_DESKTOP, &p, 1);
  }
  BuildContextMenu();
  context_menu_->RunContextMenuAt(gfx::Point(p));
}

void NativeTextfieldWin::OnCopy() {
  if (textfield_->IsPassword())
    return;

  const std::wstring text(GetSelectedText());

  if (!text.empty() && ViewsDelegate::views_delegate) {
    ScopedClipboardWriter scw(ViewsDelegate::views_delegate->GetClipboard());
    scw.WriteText(text);
  }
}

void NativeTextfieldWin::OnCut() {
  if (textfield_->read_only() || textfield_->IsPassword())
    return;

  OnCopy();

  // This replace selection will have no effect (even on the undo stack) if the
  // current selection is empty.
  ReplaceSel(L"", true);
}

LRESULT NativeTextfieldWin::OnImeChar(UINT message, WPARAM wparam, LPARAM lparam) {
  // http://crbug.com/7707: a rich-edit control may crash when it receives a
  // WM_IME_CHAR message while it is processing a WM_IME_COMPOSITION message.
  // Since view controls don't need WM_IME_CHAR messages, we prevent WM_IME_CHAR
  // messages from being dispatched to view controls via the CallWindowProc()
  // call.
  return 0;
}

LRESULT NativeTextfieldWin::OnImeStartComposition(UINT message,
                                                  WPARAM wparam,
                                                  LPARAM lparam) {
  // Users may press alt+shift or control+shift keys to change their keyboard
  // layouts. So, we retrieve the input locale identifier everytime we start
  // an IME composition.
  int language_id = PRIMARYLANGID(GetKeyboardLayout(0));
  ime_discard_composition_ =
      language_id == LANG_JAPANESE || language_id == LANG_CHINESE;
  ime_composition_start_ = 0;
  ime_composition_length_ = 0;

  return DefWindowProc(message, wparam, lparam);
}

LRESULT NativeTextfieldWin::OnImeComposition(UINT message,
                                             WPARAM wparam,
                                             LPARAM lparam) {
  text_before_change_.clear();
  LRESULT result = DefWindowProc(message, wparam, lparam);

  ime_composition_start_ = 0;
  ime_composition_length_ = 0;
  if (ime_discard_composition_) {
    // Call IMM32 functions to retrieve the position and the length of the
    // ongoing composition string and notify the OnAfterPossibleChange()
    // function that it should discard the composition string from a search
    // string. We should not call IMM32 functions in the function because it
    // is called when an IME is not composing a string.
    HIMC imm_context = ImmGetContext(m_hWnd);
    if (imm_context) {
      CHARRANGE selection;
      GetSel(selection);
      const int cursor_position =
          ImmGetCompositionString(imm_context, GCS_CURSORPOS, NULL, 0);
      if (cursor_position >= 0)
        ime_composition_start_ = selection.cpMin - cursor_position;

      const int composition_size =
          ImmGetCompositionString(imm_context, GCS_COMPSTR, NULL, 0);
      if (composition_size >= 0)
        ime_composition_length_ = composition_size / sizeof(wchar_t);

      ImmReleaseContext(m_hWnd, imm_context);
    }
  }

  OnAfterPossibleChange();
  return result;
}

LRESULT NativeTextfieldWin::OnImeEndComposition(UINT message,
                                                WPARAM wparam,
                                                LPARAM lparam) {
  // Bug 11863: Korean IMEs send a WM_IME_ENDCOMPOSITION message without
  // sending any WM_IME_COMPOSITION messages when a user deletes all
  // composition characters, i.e. a composition string becomes empty. To handle
  // this case, we need to update the find results when a composition is
  // finished or canceled.
  textfield_->SyncText();
  if (textfield_->GetController())
    textfield_->GetController()->ContentsChanged(textfield_, GetText());
  return DefWindowProc(message, wparam, lparam);
}

void NativeTextfieldWin::OnKeyDown(TCHAR key, UINT repeat_count, UINT flags) {
  // NOTE: Annoyingly, ctrl-alt-<key> generates WM_KEYDOWN rather than
  // WM_SYSKEYDOWN, so we need to check (flags & KF_ALTDOWN) in various places
  // in this function even with a WM_SYSKEYDOWN handler.

  switch (key) {
    case VK_RETURN:
      // If we are multi-line, we want to let returns through so they start a
      // new line.
      if (textfield_->IsMultiLine())
        break;
      else
        return;
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
    // Copy:  Ctrl-c is treated as copy.  Shift-Ctrl-c is not.
    // Paste: Shift-Insert and Ctrl-v are tread as paste.  Ctrl-Shift-Insert and
    //        Ctrl-Shift-v are not.
    //
    // This behavior matches most, but not all Windows programs, and largely
    // conforms to what users expect.

    case VK_DELETE:
    case 'X':
      if ((flags & KF_ALTDOWN) ||
          (GetKeyState((key == 'X') ? VK_CONTROL : VK_SHIFT) >= 0))
        break;
      if (GetKeyState((key == 'X') ? VK_SHIFT : VK_CONTROL) >= 0) {
        ScopedFreeze freeze(this, GetTextObjectModel());
        OnBeforePossibleChange();
        Cut();
        OnAfterPossibleChange();
      }
      return;

    case 'C':
      if ((flags & KF_ALTDOWN) || (GetKeyState(VK_CONTROL) >= 0))
        break;
      if (GetKeyState(VK_SHIFT) >= 0)
        Copy();
      return;

    case VK_INSERT:
      // Ignore insert by itself, so we don't turn overtype mode on/off.
      if (!(flags & KF_ALTDOWN) && (GetKeyState(VK_SHIFT) >= 0) &&
          (GetKeyState(VK_CONTROL) >= 0))
        return;
    case 'V':
      if ((flags & KF_ALTDOWN) ||
          (GetKeyState((key == 'V') ? VK_CONTROL : VK_SHIFT) >= 0))
        break;
      if (GetKeyState((key == 'V') ? VK_SHIFT : VK_CONTROL) >= 0) {
        ScopedFreeze freeze(this, GetTextObjectModel());
        OnBeforePossibleChange();
        Paste();
        OnAfterPossibleChange();
      }
      return;

    case 0xbb:  // Ctrl-'='.  Triggers subscripting, even in plain text mode.
                // We don't use VK_OEM_PLUS in case the macro isn't defined.
                // (e.g., we don't have this symbol in embeded environment).
      return;

    case VK_PROCESSKEY:
      // This key event is consumed by an IME.
      // We ignore this event because an IME sends WM_IME_COMPOSITION messages
      // when it updates the CRichEditCtrl text.
      return;
  }

  // CRichEditCtrl changes its text on WM_KEYDOWN instead of WM_CHAR for many
  // different keys (backspace, ctrl-v, ...), so we call this in both cases.
  HandleKeystroke(GetCurrentMessage()->message, key, repeat_count, flags);
}

void NativeTextfieldWin::OnLButtonDblClk(UINT keys, const CPoint& point) {
  // Save the double click info for later triple-click detection.
  tracking_double_click_ = true;
  double_click_point_ = point;
  double_click_time_ = GetCurrentMessage()->time;

  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(WM_LBUTTONDBLCLK, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, false), point.y));
  OnAfterPossibleChange();
}

void NativeTextfieldWin::OnLButtonDown(UINT keys, const CPoint& point) {
  // Check for triple click, then reset tracker.  Should be safe to subtract
  // double_click_time_ from the current message's time even if the timer has
  // wrapped in between.
  const bool is_triple_click = tracking_double_click_ &&
      win_util::IsDoubleClick(double_click_point_, point,
                              GetCurrentMessage()->time - double_click_time_);
  tracking_double_click_ = false;

  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(WM_LBUTTONDOWN, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, is_triple_click),
                           point.y));
  OnAfterPossibleChange();
}

void NativeTextfieldWin::OnLButtonUp(UINT keys, const CPoint& point) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(WM_LBUTTONUP, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, false), point.y));
  OnAfterPossibleChange();
}

void NativeTextfieldWin::OnMouseLeave() {
  SetContainsMouse(false);
}

LRESULT NativeTextfieldWin::OnMouseWheel(UINT message, WPARAM w_param,
                                         LPARAM l_param) {
  // Reroute the mouse-wheel to the window under the mouse pointer if
  // applicable.
  if (views::RerouteMouseWheel(m_hWnd, w_param, l_param))
    return 0;
  return DefWindowProc(message, w_param, l_param);;
}

void NativeTextfieldWin::OnMouseMove(UINT keys, const CPoint& point) {
  SetContainsMouse(true);
  // Clamp the selection to the visible text so the user can't drag to select
  // the "phantom newline".  In theory we could achieve this by clipping the X
  // coordinate, but in practice the edit seems to behave nondeterministically
  // with similar sequences of clipped input coordinates fed to it.  Maybe it's
  // reading the mouse cursor position directly?
  //
  // This solution has a minor visual flaw, however: if there's a visible
  // cursor at the edge of the text (only true when there's no selection),
  // dragging the mouse around outside that edge repaints the cursor on every
  // WM_MOUSEMOVE instead of allowing it to blink normally.  To fix this, we
  // special-case this exact case and discard the WM_MOUSEMOVE messages instead
  // of passing them along.
  //
  // But even this solution has a flaw!  (Argh.)  In the case where the user
  // has a selection that starts at the edge of the edit, and proceeds to the
  // middle of the edit, and the user is dragging back past the start edge to
  // remove the selection, there's a redraw problem where the change between
  // having the last few bits of text still selected and having nothing
  // selected can be slow to repaint (which feels noticeably strange).  This
  // occurs if you only let the edit receive a single WM_MOUSEMOVE past the
  // edge of the text.  I think on each WM_MOUSEMOVE the edit is repainting its
  // previous state, then updating its internal variables to the new state but
  // not repainting.  To fix this, we allow one more WM_MOUSEMOVE through after
  // the selection has supposedly been shrunk to nothing; this makes the edit
  // redraw the selection quickly so it feels smooth.
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

int NativeTextfieldWin::OnNCCalcSize(BOOL w_param, LPARAM l_param) {
  content_insets_.Set(0, 0, 0, 0);
  textfield_->CalculateInsets(&content_insets_);
  if (w_param) {
    NCCALCSIZE_PARAMS* nc_params =
        reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param);
    nc_params->rgrc[0].left += content_insets_.left();
    nc_params->rgrc[0].right -= content_insets_.right();
    nc_params->rgrc[0].top += content_insets_.top();
    nc_params->rgrc[0].bottom -= content_insets_.bottom();
  } else {
    RECT* rect = reinterpret_cast<RECT*>(l_param);
    rect->left += content_insets_.left();
    rect->right -= content_insets_.right();
    rect->top += content_insets_.top();
    rect->bottom -= content_insets_.bottom();
  }
  return 0;
}

void NativeTextfieldWin::OnNCPaint(HRGN region) {
  if (!textfield_->draw_border())
    return;

  HDC hdc = GetWindowDC();

  CRect window_rect;
  GetWindowRect(&window_rect);
  // Convert to be relative to 0x0.
  window_rect.MoveToXY(0, 0);

  ExcludeClipRect(hdc,
                  window_rect.left + content_insets_.left(),
                  window_rect.top + content_insets_.top(),
                  window_rect.right - content_insets_.right(),
                  window_rect.bottom - content_insets_.bottom());

  HBRUSH brush = CreateSolidBrush(bg_color_);
  FillRect(hdc, &window_rect, brush);
  DeleteObject(brush);

  int part;
  int state;

  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
    part = EP_EDITTEXT;

    if (!textfield_->IsEnabled())
      state = ETS_DISABLED;
    else if (textfield_->read_only())
      state = ETS_READONLY;
    else if (!contains_mouse_)
      state = ETS_NORMAL;
    else
      state = ETS_HOT;
  } else {
    part = EP_EDITBORDER_HVSCROLL;

    if (!textfield_->IsEnabled())
      state = EPSHV_DISABLED;
    else if (GetFocus() == m_hWnd)
      state = EPSHV_FOCUSED;
    else if (contains_mouse_)
      state = EPSHV_HOT;
    else
      state = EPSHV_NORMAL;
    // Vista doesn't appear to have a unique state for readonly.
  }

  int classic_state =
      (!textfield_->IsEnabled() || textfield_->read_only()) ? DFCS_INACTIVE : 0;

  gfx::NativeTheme::instance()->PaintTextField(hdc, part, state, classic_state,
                                               &window_rect, bg_color_, false,
                                               true);

  // NOTE: I tried checking the transparent property of the theme and invoking
  // drawParentBackground, but it didn't seem to make a difference.

  ReleaseDC(hdc);
}

void NativeTextfieldWin::OnNonLButtonDown(UINT keys, const CPoint& point) {
  // Interestingly, the edit doesn't seem to cancel triple clicking when the
  // x-buttons (which usually means "thumb buttons") are pressed, so we only
  // call this for M and R down.
  tracking_double_click_ = false;
  SetMsgHandled(false);
}

void NativeTextfieldWin::OnPaste() {
  if (textfield_->read_only() || !ViewsDelegate::views_delegate)
    return;

  Clipboard* clipboard = ViewsDelegate::views_delegate->GetClipboard();
  if (!clipboard->IsFormatAvailable(Clipboard::GetPlainTextWFormatType()))
    return;

  std::wstring clipboard_str;
  clipboard->ReadText(&clipboard_str);
  if (!clipboard_str.empty()) {
    std::wstring collapsed(CollapseWhitespace(clipboard_str, false));
    if (textfield_->style() & Textfield::STYLE_LOWERCASE)
      collapsed = l10n_util::ToLower(collapsed);
    // Force a Paste operation to trigger OnContentsChanged, even if identical
    // contents are pasted into the text box.
    text_before_change_.clear();
    ReplaceSel(collapsed.c_str(), true);
  }
}

void NativeTextfieldWin::OnSetFocus(HWND hwnd) {
  SetMsgHandled(FALSE);  // We still want the default processing of the message.

  views::FocusManager* focus_manager = textfield_->GetFocusManager();
  if (!focus_manager) {
    NOTREACHED();
    return;
  }
  focus_manager->SetFocusedView(textfield_);
}

void NativeTextfieldWin::OnSysChar(TCHAR ch, UINT repeat_count, UINT flags) {
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

void NativeTextfieldWin::HandleKeystroke(UINT message,
                                         TCHAR key,
                                         UINT repeat_count,
                                         UINT flags) {
  ScopedFreeze freeze(this, GetTextObjectModel());

  Textfield::Controller* controller = textfield_->GetController();
  bool handled = false;
  if (controller) {
    handled = controller->HandleKeystroke(textfield_,
        Textfield::Keystroke(message, key, repeat_count, flags));
  }

  if (!handled) {
    OnBeforePossibleChange();
    DefWindowProc(message, key, MAKELPARAM(repeat_count, flags));
    OnAfterPossibleChange();
  }
}

void NativeTextfieldWin::OnBeforePossibleChange() {
  // Record our state.
  text_before_change_ = GetText();
}

void NativeTextfieldWin::OnAfterPossibleChange() {
  // Prevent the user from selecting the "phantom newline" at the end of the
  // edit.  If they try, we just silently move the end of the selection back to
  // the end of the real text.
  CHARRANGE new_sel;
  GetSel(new_sel);
  const int length = GetTextLength();
  if (new_sel.cpMax > length) {
    new_sel.cpMax = length;
    if (new_sel.cpMin > length)
      new_sel.cpMin = length;
    SetSel(new_sel);
  }

  std::wstring new_text(GetText());
  if (new_text != text_before_change_) {
    if (ime_discard_composition_ && ime_composition_start_ >= 0 &&
        ime_composition_length_ > 0) {
      // A string retrieved with a GetText() call contains a string being
      // composed by an IME. We remove the composition string from this search
      // string.
      new_text.erase(ime_composition_start_, ime_composition_length_);
      ime_composition_start_ = 0;
      ime_composition_length_ = 0;
      if (new_text.empty())
        return;
    }
    textfield_->SyncText();
    if (textfield_->GetController())
      textfield_->GetController()->ContentsChanged(textfield_, new_text);
  }
}

LONG NativeTextfieldWin::ClipXCoordToVisibleText(LONG x,
                                                 bool is_triple_click) const {
  // Clip the X coordinate to the left edge of the text.  Careful:
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

void NativeTextfieldWin::SetContainsMouse(bool contains_mouse) {
  if (contains_mouse == contains_mouse_)
    return;

  contains_mouse_ = contains_mouse;

  if (!textfield_->draw_border())
    return;

  if (contains_mouse_) {
    // Register for notification when the mouse leaves. Need to do this so
    // that we can reset contains mouse properly.
    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_hWnd;
    tme.dwHoverTime = 0;
    TrackMouseEvent(&tme);
  }
  RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_FRAME);
}

ITextDocument* NativeTextfieldWin::GetTextObjectModel() const {
  if (!text_object_model_) {
    CComPtr<IRichEditOle> ole_interface;
    ole_interface.Attach(GetOleInterface());
    text_object_model_ = ole_interface;
  }
  return text_object_model_;
}

void NativeTextfieldWin::BuildContextMenu() {
  if (context_menu_contents_.get())
    return;
  context_menu_contents_.reset(new SimpleMenuModel(this));
  context_menu_contents_->AddItemWithStringId(IDS_APP_UNDO, IDS_APP_UNDO);
  context_menu_contents_->AddSeparator();
  context_menu_contents_->AddItemWithStringId(IDS_APP_CUT, IDS_APP_CUT);
  context_menu_contents_->AddItemWithStringId(IDS_APP_COPY, IDS_APP_COPY);
  context_menu_contents_->AddItemWithStringId(IDS_APP_PASTE, IDS_APP_PASTE);
  context_menu_contents_->AddSeparator();
  context_menu_contents_->AddItemWithStringId(IDS_APP_SELECT_ALL,
                                              IDS_APP_SELECT_ALL);
  context_menu_.reset(new Menu2(context_menu_contents_.get()));
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWrapper, public:

// static
NativeTextfieldWrapper* NativeTextfieldWrapper::CreateWrapper(
    Textfield* field) {
  return new NativeTextfieldWin(field);
}

}  // namespace views
