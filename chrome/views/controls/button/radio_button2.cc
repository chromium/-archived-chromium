// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/views/controls/button/radio_button2.h"

#include "chrome/views/widget/root_view.h"

namespace views {

// static
const char RadioButton2::kViewClassName[] = "chrome/views/RadioButton";

////////////////////////////////////////////////////////////////////////////////
// RadioButton, public:

RadioButton2::RadioButton2() {
}

RadioButton2::RadioButton2(ButtonListener* listener) : Checkbox2(listener) {
}

RadioButton2::RadioButton2(ButtonListener* listener, const std::wstring& label)
    : Checkbox2(listener, label) {
}

RadioButton2::RadioButton2(ButtonListener* listener,
                           const std::wstring& label,
                           int group_id)
    : Checkbox2(listener, label) {
  SetGroup(group_id);
}

RadioButton2::~RadioButton2() {
}

////////////////////////////////////////////////////////////////////////////////
// RadioButton, Checkbox overrides:

void RadioButton2::SetChecked(bool checked) {
  if (checked == RadioButton2::checked())
    return;
  if (checked) {
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
          RadioButton2* peer = static_cast<RadioButton2*>(*i);
          peer->SetChecked(false);
        }
      }
    }
  }
  Checkbox2::SetChecked(checked);

}

////////////////////////////////////////////////////////////////////////////////
// RadioButton, View overrides:

View* RadioButton2::GetSelectedViewForGroup(int group_id) {
  std::vector<View*> views;
  GetRootView()->GetViewsWithGroup(group_id, &views);
  if (views.empty())
    return NULL;

  for (std::vector<View*>::const_iterator iter = views.begin();
       iter != views.end(); ++iter) {
    RadioButton2* radio_button = static_cast<RadioButton2*>(*iter);
    if (radio_button->checked())
      return radio_button;
  }
  return NULL;
}

bool RadioButton2::IsGroupFocusTraversable() const {
  // When focusing a radio button with tab/shift+tab, only the selected button
  // from the group should be focused.
  return false;
}

std::string RadioButton2::GetClassName() const {
  return kViewClassName;
}

////////////////////////////////////////////////////////////////////////////////
// RadioButton, NativeButton overrides:

void RadioButton2::CreateWrapper() {
  native_wrapper_ = NativeButtonWrapper::CreateRadioButtonWrapper(this);
  native_wrapper_->UpdateLabel();
  native_wrapper_->UpdateChecked();
}

}  // namespace views
