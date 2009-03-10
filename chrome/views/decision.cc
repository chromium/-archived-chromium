// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/views/decision.h"

#include "chrome/common/resource_bundle.h"
#include "chrome/views/label.h"
#include "chrome/views/grid_layout.h"

using namespace std;

namespace views {

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
  virtual void ButtonPressed(NativeButton* sender);

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

void Decision::ViewHierarchyChanged(bool is_add, View *parent, View *child) {
  if (is_add && child == this) {
    // Layout when this is added so that the buttons are laid out correctly.
    Layout();
  }
}

void Decision::Layout() {
  gfx::Rect lb = GetLocalBounds(false);

  // Resize for padding.
  lb.Inset(kPaddingEdge, kPaddingEdge);
  int width = lb.width();

  gfx::Point position = lb.origin();
  gfx::Size size = title_label_->GetPreferredSize();
  title_label_->SetBounds(position.x(), position.y(), width, size.height());
  position.set_y(position.y() + size.height() + kSpacingInfoBottom);

  size.set_height(details_label_->GetHeightForWidth(width));
  details_label_->SetBounds(position.x(), position.y(), width, size.height());
  position.set_y(position.y() + size.height() + kSpacingInfoBottom);

  for (std::vector<Option*>::const_iterator iter = options_.begin();
       iter != options_.end(); ++iter) {
    Option* option = *iter;
    size = option->GetPreferredSize();
    option->SetBounds(position.x(), position.y(), width, size.height());
    option->Layout();
    position.set_y(position.y() + size.height() + kSpacingInfoBottom);
  }
}

gfx::Size Decision::GetPreferredSize() {
  int width = 0;
  int height = 0;

  // We need to find the largest width from the title and the options, as the
  // details label is multi-line and we need to known its width in order to
  // compute its height.
  gfx::Size size = title_label_->GetPreferredSize();
  width = size.width();
  height = size.height() + kSpacingInfoBottom;

  for (std::vector<Option*>::const_iterator iter = options_.begin();
       iter != options_.end(); ++iter) {
    size = (*iter)->GetPreferredSize();
    if (size.width() > width)
      width = size.width();
    height += size.height() + kSpacingInfoBottom;
  }

  // Now we can compute the details label height.
  height += details_label_->GetHeightForWidth(width) + kSpacingInfoBottom;

  return gfx::Size(width + 2 * kPaddingEdge, height + 2 * kPaddingEdge);
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

void Option::ButtonPressed(NativeButton* sender) {
  controller_->ExecuteCommand(command_id_);
}

}  // namespace views
