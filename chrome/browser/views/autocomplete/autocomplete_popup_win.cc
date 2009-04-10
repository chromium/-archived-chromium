// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/autocomplete/autocomplete_popup_win.h"

#include "chrome/browser/autocomplete/autocomplete_edit_view_win.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/common/win_util.h"

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, public:

AutocompletePopupWin::AutocompletePopupWin(const ChromeFont& font,
                                           AutocompleteEditViewWin* edit_view,
                                           AutocompleteEditModel* edit_model,
                                           Profile* profile)
    : model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view) {
  set_delete_on_destroy(false);
  set_window_style(WS_POPUP | WS_CLIPCHILDREN);
  set_window_ex_style(WS_EX_TOOLWINDOW);
}

AutocompletePopupWin::~AutocompletePopupWin() {
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, AutocompletePopupView overrides:

bool AutocompletePopupWin::IsOpen() const {
  return false;
}

void AutocompletePopupWin::InvalidateLine(size_t line) {
}

void AutocompletePopupWin::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();
  if (result.empty()) {
    // No matches, close any existing popup.
    if (::IsWindow(GetNativeView()))
      Hide();
    return;
  }

  gfx::Rect popup_bounds = GetPopupScreenBounds();
  if (!::IsWindow(GetNativeView())) {
    // Create the popup
    HWND parent_hwnd = edit_view_->parent_view()->GetWidget()->GetNativeView();
    Init(parent_hwnd, popup_bounds, false);
    Show();
  } else {
    // Move the popup
    int flags = 0;
    if (!IsVisible())
      flags |= SWP_SHOWWINDOW;
    win_util::SetChildBounds(GetNativeView(), NULL, NULL, popup_bounds, 0,
                             flags);
  }
}

void AutocompletePopupWin::OnHoverEnabledOrDisabled(bool disabled) {
}

void AutocompletePopupWin::PaintUpdatesNow() {
}

AutocompletePopupModel* AutocompletePopupWin::GetModel() {
  return model_.get();
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, private:

gfx::Rect AutocompletePopupWin::GetPopupScreenBounds() const {
  gfx::Point popup_origin;
  views::View::ConvertPointToScreen(edit_view_->parent_view(), &popup_origin);

  gfx::Rect popup_bounds(popup_origin,
                         edit_view_->parent_view()->bounds().size());

  // The popup starts at the bottom of the edit.
  popup_bounds.set_y(popup_bounds.bottom());
  popup_bounds.set_height(100);
  return popup_bounds;
}

