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

#include "chrome/views/radio_button.h"

#include "chrome/views/label.h"
#include "chrome/views/hwnd_view.h"
#include "chrome/views/root_view.h"

namespace ChromeViews {

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
                            0, 0, GetWidth(), GetHeight(),
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

void RadioButton::GetPreferredSize(CSize *out) {
  label_->GetPreferredSize(out);
  out->cy = std::max(static_cast<int>(out->cy + kFocusPaddingVertical * 2),
                     kRadioHeight) ;
  out->cx += kRadioToLabel + kRadioWidth + kFocusPaddingHorizontal * 2;
}

void RadioButton::Layout() {
  int label_x = GetTextIndent();
  label_->SetBounds(label_x, 0, GetWidth() - label_x, GetHeight());
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

}
