// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/views/controls/button/checkbox2.h"

#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/views/controls/label.h"

namespace views {

// static
const char Checkbox2::kViewClassName[] = "chrome/views/Checkbox";

static const int kCheckboxLabelSpacing = 4;
static const int kLabelFocusPaddingHorizontal = 2;
static const int kLabelFocusPaddingVertical = 1;

////////////////////////////////////////////////////////////////////////////////
// Checkbox2, public:

Checkbox2::Checkbox2() : NativeButton2(NULL), checked_(false) {
  Init(std::wstring());
}

Checkbox2::Checkbox2(ButtonListener* listener)
    : NativeButton2(listener),
      checked_(false) {
  Init(std::wstring());
}

Checkbox2::Checkbox2(ButtonListener* listener, const std::wstring& label)
    : NativeButton2(listener, label),
      checked_(false) {
  Init(label);
}

Checkbox2::~Checkbox2() {
}

void Checkbox2::SetMultiLine(bool multiline) {
  label_->SetMultiLine(multiline);
}

void Checkbox2::SetChecked(bool checked) {
  if (checked_ == checked)
    return;
  checked_ = checked;
  native_wrapper_->UpdateChecked();
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox2, View overrides:

gfx::Size Checkbox2::GetPreferredSize() {
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

void Checkbox2::Layout() {
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

void Checkbox2::Paint(ChromeCanvas* canvas) {
  // Paint the focus border manually since we don't want to send actual focus
  // in to the inner view.
  if (HasFocus()) {
    gfx::Rect label_bounds = label_->bounds();
    canvas->DrawFocusRect(
        MirroredLeftPointForRect(label_bounds) - kLabelFocusPaddingHorizontal,
        0,
        label_bounds.width() + kLabelFocusPaddingHorizontal * 2,
        label_bounds.height() - kLabelFocusPaddingVertical * 2);
  }
}

View* Checkbox2::GetViewForPoint(const gfx::Point& point) {
  return GetViewForPoint(point, false);
}

View* Checkbox2::GetViewForPoint(const gfx::Point& point,
                                 bool can_create_floating) {
  return GetLocalBounds(true).Contains(point) ? this : NULL;
}

void Checkbox2::OnMouseEntered(const MouseEvent& e) {
  native_wrapper_->SetPushed(HitTestLabel(e));
}

void Checkbox2::OnMouseMoved(const MouseEvent& e) {
  native_wrapper_->SetPushed(HitTestLabel(e));
}

void Checkbox2::OnMouseExited(const MouseEvent& e) {
  native_wrapper_->SetPushed(false);
}

bool Checkbox2::OnMousePressed(const MouseEvent& e) {
  native_wrapper_->SetPushed(HitTestLabel(e));
  return true;
}

void Checkbox2::OnMouseReleased(const MouseEvent& e, bool canceled) {
  native_wrapper_->SetPushed(false);
  if (!canceled & HitTestLabel(e)) {
    SetChecked(!checked());
    ButtonPressed();
  }
}

std::string Checkbox2::GetClassName() const {
  return kViewClassName;
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox2, NativeButton2 overrides:

void Checkbox2::CreateWrapper() {
  native_wrapper_ = NativeButtonWrapper::CreateCheckboxWrapper(this);
  native_wrapper_->UpdateLabel();
  native_wrapper_->UpdateChecked();
}

void Checkbox2::InitBorder() {
  // No border, so we do nothing.
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox2, private:

void Checkbox2::Init(const std::wstring& label_text) {
  SetFocusable(true);
  set_minimum_size(gfx::Size(0, 0));
  label_ = new Label(label_text);
  label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  AddChildView(label_);
}

bool Checkbox2::HitTestLabel(const MouseEvent& e) {
  gfx::Point tmp(e.location());
  ConvertPointToView(this, label_, &tmp);
  return label_->HitTest(tmp);
}

}  // namespace views
