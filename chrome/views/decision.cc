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

#include <string>

#include "chrome/views/decision.h"

#include "chrome/common/resource_bundle.h"
#include "chrome/views/label.h"
#include "chrome/views/grid_layout.h"

using namespace std;

namespace ChromeViews {

static const int kPaddingEdge = 10;
static const int kSpacingInfoBottom = 20;

class Option : public View,
               public NativeButton::Listener {
 public:
  Option(int command_id,
         const std::wstring& description,
         const std::wstring& action,
         Controller* controller);

  // NativeButton::Listener methods:
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

 private:
  int command_id_;
  Controller* controller_;
};

Decision::Decision(const std::wstring& title,
                   const std::wstring& details,
                   Controller* controller)
    : controller_(controller) {
  // The main message.
  title_label_ = new Label(title);
  title_label_->SetFont(
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::LargeFont));
  title_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  AddChildView(title_label_);

  // The detailed description.
  details_label_ = new Label(details);
  details_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  details_label_->SetMultiLine(true);
  AddChildView(details_label_);
}

void Decision::AppendOption(int command_id,
                            const std::wstring& description,
                            const std::wstring& action) {
  Option* option = new Option(command_id, description, action, controller_);
  options_.push_back(option);
  AddChildView(option);
}

void Decision::DidChangeBounds(const CRect& old_bounds,
                               const CRect& new_bounds) {
  Layout();
}

void Decision::ViewHierarchyChanged(bool is_add, View *parent, View *child) {
  if (is_add && child == this) {
    // Layout when this is added so that the buttons are laid out correctly.
    Layout();
  }
}

void Decision::Layout() {
  CRect lb;
  GetLocalBounds(&lb, false);

  // Resize for padding.
  lb.DeflateRect(kPaddingEdge, kPaddingEdge);
  int width = lb.Width();

  CPoint position(lb.TopLeft());
  CSize size;
  title_label_->GetPreferredSize(&size);
  title_label_->SetBounds(position.x, position.y, width, size.cy);
  position.y += size.cy + kSpacingInfoBottom;

  size.cy = details_label_->GetHeightForWidth(width);
  details_label_->SetBounds(position.x, position.y, width, size.cy);
  position.y += size.cy + kSpacingInfoBottom;

  for (std::vector<Option*>::const_iterator iter = options_.begin();
       iter != options_.end(); ++iter) {
    Option* option = *iter;
    option->GetPreferredSize(&size);
    option->SetBounds(position.x, position.y, width, size.cy);
    option->Layout();
    position.y += size.cy + kSpacingInfoBottom;
  }
}

void Decision::GetPreferredSize(CSize *out) {
  int width = 0;
  int height = 0;

  // We need to find the largest width from the title and the options, as the
  // details label is multi-line and we need to known its width in order to
  // compute its height.
  CSize size;
  title_label_->GetPreferredSize(&size);
  width = size.cx;
  height = size.cy + kSpacingInfoBottom;

  for (std::vector<Option*>::const_iterator iter = options_.begin();
       iter != options_.end(); ++iter) {
    (*iter)->GetPreferredSize(&size);
    if (size.cx > width)
      width = size.cx;
    height += size.cy + kSpacingInfoBottom;
  }

  // Now we can compute the details label height.
  height += details_label_->GetHeightForWidth(width) + kSpacingInfoBottom;

  out->cx = width + 2 * kPaddingEdge;
  out->cy = height + 2 * kPaddingEdge;
}

Option::Option(int command_id,
               const std::wstring& description,
               const std::wstring& action,
               Controller* controller)
    : command_id_(command_id),
      controller_(controller) {

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(GridLayout::FILL, GridLayout::CENTER,
                     1, GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, 10);
  columns->AddColumn(GridLayout::TRAILING, GridLayout::CENTER,
                     0, GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  Label* label = new Label(description);
  label->SetHorizontalAlignment(Label::ALIGN_LEFT);
  layout->AddView(label);

  // A button to perform the action.
  NativeButton* button = new NativeButton(action);
  button->SetListener(this);
  layout->AddView(button);
}

void Option::ButtonPressed(ChromeViews::NativeButton* sender) {
  controller_->ExecuteCommand(command_id_);
}

} // namespace ChromeViews
