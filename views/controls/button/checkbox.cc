// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "views/controls/button/checkbox.h"

#include "app/gfx/canvas.h"
#include "views/controls/label.h"

namespace views {

// static
const char Checkbox::kViewClassName[] = "views/Checkbox";

static const int kCheckboxLabelSpacing = 4;
static const int kLabelFocusPaddingHorizontal = 2;
static const int kLabelFocusPaddingVertical = 1;

////////////////////////////////////////////////////////////////////////////////
// Checkbox, public:

Checkbox::Checkbox() : NativeButton(NULL), checked_(false) {
  Init(std::wstring());
}

Checkbox::Checkbox(const std::wstring& label)
    : NativeButton(NULL, label),
      checked_(false) {
  Init(label);
}

Checkbox::~Checkbox() {
}

void Checkbox::SetMultiLine(bool multiline) {
  label_->SetMultiLine(multiline);
}

void Checkbox::SetChecked(bool checked) {
  if (checked_ == checked)
    return;
  checked_ = checked;
  if (native_wrapper_)
    native_wrapper_->UpdateChecked();
}

// static
int Checkbox::GetTextIndent() {
  return NativeButtonWrapper::GetFixedWidth() + kCheckboxLabelSpacing;
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox, View overrides:

gfx::Size Checkbox::GetPreferredSize() {
  if (!native_wrapper_)
    return gfx::Size();

  gfx::Size prefsize = native_wrapper_->GetView()->GetPreferredSize();
  prefsize.set_width(
      prefsize.width() + kCheckboxLabelSpacing +
          kLabelFocusPaddingHorizontal * 2);
  gfx::Size label_prefsize = label_->GetPreferredSize();
  prefsize.set_width(prefsize.width() + label_prefsize.width());
  prefsize.set_height(
      std::max(prefsize.height(),
               label_prefsize.height() + kLabelFocusPaddingVertical * 2));
  return prefsize;
}

void Checkbox::Layout() {
  if (!native_wrapper_)
    return;

  gfx::Size checkmark_prefsize = native_wrapper_->GetView()->GetPreferredSize();
  int label_x = checkmark_prefsize.width() + kCheckboxLabelSpacing +
      kLabelFocusPaddingHorizontal;
  label_->SetBounds(
      label_x, 0, std::max(0, width() - label_x - kLabelFocusPaddingHorizontal),
      height());
  int first_line_height = label_->GetFont().height();
  native_wrapper_->GetView()->SetBounds(
      0, ((first_line_height - checkmark_prefsize.height()) / 2),
      checkmark_prefsize.width(), checkmark_prefsize.height());
  native_wrapper_->GetView()->Layout();
}

void Checkbox::PaintFocusBorder(gfx::Canvas* canvas) {
  // Our focus border is rendered by the label, so we don't do anything here.
}

View* Checkbox::GetViewForPoint(const gfx::Point& point) {
  return GetViewForPoint(point, false);
}

View* Checkbox::GetViewForPoint(const gfx::Point& point,
                                bool can_create_floating) {
  return GetLocalBounds(true).Contains(point) ? this : NULL;
}

void Checkbox::OnMouseEntered(const MouseEvent& e) {
  native_wrapper_->SetPushed(HitTestLabel(e));
}

void Checkbox::OnMouseMoved(const MouseEvent& e) {
  native_wrapper_->SetPushed(HitTestLabel(e));
}

void Checkbox::OnMouseExited(const MouseEvent& e) {
  native_wrapper_->SetPushed(false);
}

bool Checkbox::OnMousePressed(const MouseEvent& e) {
  native_wrapper_->SetPushed(HitTestLabel(e));
  return true;
}

void Checkbox::OnMouseReleased(const MouseEvent& e, bool canceled) {
  native_wrapper_->SetPushed(false);
  if (!canceled && HitTestLabel(e)) {
    SetChecked(!checked());
    ButtonPressed();
  }
}

bool Checkbox::OnMouseDragged(const MouseEvent& e) {
  return false;
}

void Checkbox::WillGainFocus() {
  label_->set_paint_as_focused(true);
}

void Checkbox::WillLoseFocus() {
  label_->set_paint_as_focused(false);
}

std::string Checkbox::GetClassName() const {
  return kViewClassName;
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox, NativeButton overrides:

void Checkbox::CreateWrapper() {
  native_wrapper_ = NativeButtonWrapper::CreateCheckboxWrapper(this);
  native_wrapper_->UpdateLabel();
  native_wrapper_->UpdateChecked();
}

void Checkbox::InitBorder() {
  // No border, so we do nothing.
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox, protected:

bool Checkbox::HitTestLabel(const MouseEvent& e) {
  gfx::Point tmp(e.location());
  ConvertPointToView(this, label_, &tmp);
  return label_->HitTest(tmp);
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox, private:

void Checkbox::Init(const std::wstring& label_text) {
  set_minimum_size(gfx::Size(0, 0));
  label_ = new Label(label_text);
  label_->set_has_focus_border(true);
  label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  AddChildView(label_);
}

}  // namespace views
