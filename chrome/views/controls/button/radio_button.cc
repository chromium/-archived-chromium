// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/controls/button/radio_button.h"

#include "chrome/views/controls/label.h"
#include "chrome/views/controls/hwnd_view.h"
#include "chrome/views/widget/root_view.h"

namespace views {

// FIXME(ACW) there got be a better way to find out the check box sizes
static int kRadioWidth = 13;
static int kRadioHeight = 13;
static int kRadioToLabel = 4;

const char RadioButton::kViewClassName[] = "chrome/views/RadioButton";

RadioButton::RadioButton(const std::wstring& label, int group_id)
    : CheckBox(label) {
  SetGroup(group_id);
}

RadioButton::~RadioButton() {
}

HWND RadioButton::CreateNativeControl(HWND parent_container) {
  HWND r = ::CreateWindowEx(GetAdditionalExStyle(),
                            L"BUTTON",
                            L"",
                            WS_CHILD | BS_RADIOBUTTON ,
                            0, 0, width(), height(),
                            parent_container, NULL, NULL, NULL);
  ConfigureNativeButton(r);
  return r;
}

LRESULT RadioButton::OnCommand(UINT code, int id, HWND source) {
  // Radio buttons can't be toggled off once selected except by clicking on
  // another radio button within the same group, so we override this from
  // CheckBox to prevent this from happening.
  if (code == BN_CLICKED) {
    RequestFocus();
    if (!IsSelected()) {
      SetIsSelected(true);
      return NativeButton::OnCommand(code, id, source);
    }
  }
  return 0;
}

// static
int RadioButton::GetTextIndent() {
  return kRadioWidth + kRadioToLabel + kFocusPaddingHorizontal;
}


std::string RadioButton::GetClassName() const {
  return kViewClassName;
}

gfx::Size RadioButton::GetPreferredSize() {
  gfx::Size prefsize = label_->GetPreferredSize();
  prefsize.set_height(std::max(prefsize.height() + kFocusPaddingVertical * 2,
                               kRadioHeight));
  prefsize.Enlarge(kRadioToLabel + kRadioWidth + kFocusPaddingHorizontal * 2,
                   0);
  return prefsize;
}

void RadioButton::Layout() {
  int label_x = GetTextIndent();
  label_->SetBounds(label_x, 0, width() - label_x, height());
  if (hwnd_view_) {
    int first_line_height = label_->GetFont().height();
    hwnd_view_->SetBounds(0, ((first_line_height - kRadioHeight) / 2) + 1,
                          kRadioWidth, kRadioHeight);
    hwnd_view_->UpdateHWNDBounds();
  }
}

void RadioButton::SetIsSelected(bool f) {
  if (f != IsSelected()) {
    if (f) {
      // We can't just get the root view here because sometimes the radio
      // button isn't attached to a root view (e.g., if it's part of a tab page
      // that is currently not active).
      View* container = GetParent();
      while (container && container->GetParent())
        container = container->GetParent();
      if (container) {
        std::vector<View*> other;
        container->GetViewsWithGroup(GetGroup(), &other);
        std::vector<View*>::iterator i;
        for (i = other.begin(); i != other.end(); ++i) {
          if (*i != this) {
            RadioButton* peer = static_cast<RadioButton*>(*i);
            peer->SetIsSelected(false);
          }
        }
      }
    }
    CheckBox::SetIsSelected(f);
  }
}

View* RadioButton::GetSelectedViewForGroup(int group_id) {
  std::vector<View*> views;
  GetRootView()->GetViewsWithGroup(group_id, &views);
  if (views.empty())
    return NULL;

  for (std::vector<View*>::const_iterator iter = views.begin();
       iter != views.end(); ++iter) {
    RadioButton* radio_button = static_cast<RadioButton*>(*iter);
    if (radio_button->IsSelected())
      return radio_button;
  }
  return NULL;
}

}  // namespace views
