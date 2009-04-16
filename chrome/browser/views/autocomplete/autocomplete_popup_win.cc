// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/autocomplete/autocomplete_popup_win.h"

#include "chrome/browser/autocomplete/autocomplete_edit_view_win.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/views/autocomplete/autocomplete_popup_contents_view.h"
#include "chrome/common/gfx/insets.h"
#include "chrome/common/win_util.h"

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, public:

AutocompletePopupWin::AutocompletePopupWin(
    const ChromeFont& font,
    AutocompleteEditViewWin* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile,
    AutocompletePopupPositioner* popup_positioner)
    : model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view),
      popup_positioner_(popup_positioner),
      contents_(new AutocompletePopupContentsView(this)) {
  set_delete_on_destroy(false);
  set_window_style(WS_POPUP | WS_CLIPCHILDREN);
  set_window_ex_style(WS_EX_TOOLWINDOW | WS_EX_LAYERED);
}

AutocompletePopupWin::~AutocompletePopupWin() {
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, AutocompletePopupView overrides:

bool AutocompletePopupWin::IsOpen() const {
  return IsWindow() && IsVisible();
}

void AutocompletePopupWin::InvalidateLine(size_t line) {
  contents_->InvalidateLine(static_cast<int>(line));
}

void AutocompletePopupWin::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();
  if (result.empty()) {
    // No matches, close any existing popup.
    if (::IsWindow(GetNativeView()))
      Hide();
    return;
  }

  gfx::Rect popup_bounds = GetPopupBounds();
  if (!::IsWindow(GetNativeView())) {
    // Create the popup
    HWND parent_hwnd = edit_view_->parent_view()->GetWidget()->GetNativeView();
    Init(parent_hwnd, popup_bounds, false);
    SetContentsView(contents_);

    // When an IME is attached to the rich-edit control, retrieve its window
    // handle and show this popup window under the IME windows.
    // Otherwise, show this popup window under top-most windows.
    // TODO(hbono): http://b/1111369 if we exclude this popup window from the
    // display area of IME windows, this workaround becomes unnecessary.
    HWND ime_window = ImmGetDefaultIMEWnd(edit_view_->m_hWnd);
    SetWindowPos(ime_window ? ime_window : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  } else {
    // Move the popup
    int flags = SWP_NOACTIVATE;
    if (!IsVisible())
      flags |= SWP_SHOWWINDOW;
    win_util::SetChildBounds(GetNativeView(), NULL, NULL, popup_bounds, 0,
                             flags);
  }
  contents_->SetAutocompleteResult(result);
}

void AutocompletePopupWin::OnHoverEnabledOrDisabled(bool disabled) {
  // TODO(beng): remove this from the interface.
}

void AutocompletePopupWin::PaintUpdatesNow() {
  // TODO(beng): remove this from the interface.
}

AutocompletePopupModel* AutocompletePopupWin::GetModel() {
  return model_.get();
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, WidgetWin overrides:

LRESULT AutocompletePopupWin::OnMouseActivate(HWND window, UINT hit_test,
                                              UINT mouse_message) {
  return MA_NOACTIVATE;
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, private:

gfx::Rect AutocompletePopupWin::GetPopupBounds() const {
  gfx::Insets insets;
  contents_->border()->GetInsets(&insets);
  gfx::Rect contents_bounds = popup_positioner_->GetPopupBounds();
  contents_bounds.set_height(100); // TODO(beng): size to contents (once we have
                                   //             contents!)
  contents_bounds.Inset(-insets.left(), -insets.top(), -insets.right(),
                        -insets.bottom());
  return contents_bounds;
}

void AutocompletePopupWin::OpenIndex(int index,
                                     WindowOpenDisposition disposition) {
  const AutocompleteMatch& match = model_->result().match_at(index);
  // OpenURL() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant strings out to
  // make sure they stay alive until the call completes.
  const GURL url(match.destination_url);
  std::wstring keyword;
  const bool is_keyword_hint = model_->GetKeywordForMatch(match, &keyword);
  edit_view_->OpenURL(url, disposition, match.transition, GURL(), index,
                      is_keyword_hint ? std::wstring() : keyword);  
}

void AutocompletePopupWin::SetHoveredLine(int index) {
  model_->SetHoveredLine(index);
}

void AutocompletePopupWin::SetSelectedLine(int index, bool revert_to_default) {
  model_->SetSelectedLine(index, revert_to_default);
}
