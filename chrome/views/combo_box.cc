// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/combo_box.h"

#include "base/gfx/native_theme.h"
#include "base/gfx/rect.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"

// Limit how small a combobox can be.
static const int kMinComboboxWidth = 148;

// Add a couple extra pixels to the widths of comboboxes and combobox
// dropdowns so that text isn't too crowded.
static const int kComboboxExtraPaddingX = 6;

namespace views {

ComboBox::ComboBox(Model* model)
    : model_(model), selected_item_(0), listener_(NULL), content_width_(0) {
}

ComboBox::~ComboBox() {
}

void ComboBox::SetListener(Listener* listener) {
  listener_ = listener;
}

gfx::Size ComboBox::GetPreferredSize() {
  HWND hwnd = GetNativeControlHWND();
  if (!hwnd)
    return gfx::Size();

  COMBOBOXINFO cbi;
  memset(reinterpret_cast<unsigned char*>(&cbi), 0, sizeof(cbi));
  cbi.cbSize = sizeof(cbi);
  // Note: Don't use CB_GETCOMBOBOXINFO since that crashes on WOW64 systems
  // when you have a global message hook installed.
  GetComboBoxInfo(hwnd, &cbi);
  gfx::Rect rect_item(cbi.rcItem);
  gfx::Rect rect_button(cbi.rcButton);
  gfx::Size border = gfx::NativeTheme::instance()->GetThemeBorderSize(
      gfx::NativeTheme::MENULIST);

  // The padding value of '3' is the xy offset from the corner of the control
  // to the corner of rcItem.  It does not seem to be queryable from the theme.
  // It is consistent on all versions of Windows from 2K to Vista, and is
  // invariant with respect to the combobox border size.  We could conceivably
  // get this number from rect_item.x, but it seems fragile to depend on position
  // here, inside of the layout code.
  const int kItemOffset = 3;
  int item_to_button_distance = std::max(kItemOffset - border.width(), 0);

  // The cx computation can be read as measuring from left to right.
  int pref_width = std::max(kItemOffset + content_width_ +
                                kComboboxExtraPaddingX +
                                item_to_button_distance + rect_button.width() +
                                 border.width(), kMinComboboxWidth);
  // The two arguments to ::max below should be typically be equal.
  int pref_height = std::max(rect_item.height() + 2 * kItemOffset,
                             rect_button.height() + 2 * border.height());
  return gfx::Size(pref_width, pref_height);
}

HWND ComboBox::CreateNativeControl(HWND parent_container) {
  HWND r = ::CreateWindowEx(GetAdditionalExStyle(), L"COMBOBOX", L"",
                            WS_CHILD | WS_VSCROLL | CBS_DROPDOWNLIST,
                            0, 0, width(), height(),
                            parent_container, NULL, NULL, NULL);
  HFONT font = ResourceBundle::GetSharedInstance().
      GetFont(ResourceBundle::BaseFont).hfont();
  SendMessage(r, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
  UpdateComboBoxFromModel(r);
  return r;
}

LRESULT ComboBox::OnCommand(UINT code, int id, HWND source) {
  HWND hwnd = GetNativeControlHWND();
  if (!hwnd)
    return 0;

  if (code == CBN_SELCHANGE && source == hwnd) {
    LRESULT r = ::SendMessage(hwnd, CB_GETCURSEL, 0, 0);
    if (r != CB_ERR) {
      int prev_selected_item = selected_item_;
      selected_item_ = static_cast<int>(r);
      if (listener_)
        listener_->ItemChanged(this, prev_selected_item, selected_item_);
    }
  }
  return 0;
}

LRESULT ComboBox::OnNotify(int w_param, LPNMHDR l_param) {
  return 0;
}

void ComboBox::UpdateComboBoxFromModel(HWND hwnd) {
  ::SendMessage(hwnd, CB_RESETCONTENT, 0, 0);
  ChromeFont font = ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont);
  int max_width = 0;
  int num_items = model_->GetItemCount(this);
  for (int i = 0; i < num_items; ++i) {
    const std::wstring& text = model_->GetItemAt(this, i);

    // Inserting the Unicode formatting characters if necessary so that the
    // text is displayed correctly in right-to-left UIs.
    std::wstring localized_text;
    const wchar_t* text_ptr = text.c_str();
    if (l10n_util::AdjustStringForLocaleDirection(text, &localized_text))
      text_ptr = localized_text.c_str();

    ::SendMessage(hwnd, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text_ptr));
    max_width = std::max(max_width, font.GetStringWidth(text));

  }
  content_width_ = max_width;

  if (num_items > 0) {
    ::SendMessage(hwnd, CB_SETCURSEL, selected_item_, 0);

    // Set the width for the drop down while accounting for the scrollbar and
    // borders.
    if (num_items > ComboBox_GetMinVisible(hwnd))
      max_width += ::GetSystemMetrics(SM_CXVSCROLL);
    // SM_CXEDGE would not be correct here, since the dropdown is flat, not 3D.
    int kComboboxDropdownBorderSize = 1;
    max_width += 2 * kComboboxDropdownBorderSize + kComboboxExtraPaddingX;
    ::SendMessage(hwnd, CB_SETDROPPEDWIDTH, max_width, 0);
  }
}

void ComboBox::ModelChanged() {
  HWND hwnd = GetNativeControlHWND();
  if (!hwnd)
    return;
  selected_item_ = std::min(0, model_->GetItemCount(this));
  UpdateComboBoxFromModel(hwnd);
}

void ComboBox::SetSelectedItem(int index) {
  selected_item_ = index;
  HWND hwnd = GetNativeControlHWND();
  if (!hwnd)
    return;

  // Note that we use CB_SETCURSEL and not CB_SELECTSTRING because on RTL
  // locales the strings we get from our ComboBox::Model might be augmented
  // with Unicode directionality marks before we insert them into the combo box
  // and therefore we can not assume that the string we get from
  // ComboBox::Model can be safely searched for and selected (which is what
  // CB_SELECTSTRING does).
  ::SendMessage(hwnd, CB_SETCURSEL, selected_item_, 0);
}

int ComboBox::GetSelectedItem() {
  return selected_item_;
}

}  // namespace views
