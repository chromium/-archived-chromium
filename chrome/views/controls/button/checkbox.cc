// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/controls/button/checkbox.h"

#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/views/controls/button/checkbox.h"
#include "chrome/views/controls/hwnd_view.h"
#include "chrome/views/controls/label.h"

// FIXME(ACW) there got be a better way to find out the check box sizes
static int kCheckBoxWidth = 13;
static int kCheckBoxHeight = 13;
static int kCheckBoxToLabel = 4;

namespace views {

// Horizontal focus padding.
const int CheckBox::kFocusPaddingHorizontal = 2;
const int CheckBox::kFocusPaddingVertical = 1;

const char CheckBox::kViewClassName[] = "chrome/views/CheckBox";

CheckBox::CheckBox(const std::wstring& label)
    : NativeButton(label),
      is_selected_(false) {
  // Note: we paint the label as a floating view
  SetMinSizeFromDLUs(gfx::Size(0, 0));
  label_ = new Label(label);
  label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
}

CheckBox::~CheckBox() {
  delete label_;
}

void CheckBox::SetMultiLine(bool multi_line) {
  label_->SetMultiLine(multi_line);
}

// static
int CheckBox::GetTextIndent() {
  return kCheckBoxWidth + kCheckBoxToLabel + kFocusPaddingHorizontal;
}

void CheckBox::SetIsSelected(bool f) {
  if (f != is_selected_) {
    is_selected_ = f;
    UpdateNativeButton();
  }
}

bool CheckBox::IsSelected() const {
  return is_selected_;
}

std::string CheckBox::GetClassName() const {
  return kViewClassName;
}

void CheckBox::Layout() {
  int label_x = GetTextIndent();
  label_->SetBounds(label_x, 0, std::max(0, width() - label_x), height());
  if (hwnd_view_) {
    int first_line_height = label_->GetFont().height();
    hwnd_view_->SetBounds(0, ((first_line_height - kCheckBoxHeight) / 2) + 1,
                          kCheckBoxWidth, kCheckBoxHeight);
    hwnd_view_->Layout();
  }
}

void CheckBox::ComputeTextRect(gfx::Rect* out) {
  gfx::Size s = label_->GetPreferredSize();
  out->set_x(GetTextIndent());
  out->set_y(kFocusPaddingVertical);
  int new_width = std::min(width() - (kCheckBoxWidth + kCheckBoxToLabel),
                           s.width());
  out->set_width(std::max(0, new_width));
  out->set_height(s.height());
}

void CheckBox::Paint(ChromeCanvas* canvas) {
  gfx::Rect r;
  ComputeTextRect(&r);
  // Paint the focus border if any.
  if (HasFocus()) {
    // Mirror left point for rectangle to draw focus for RTL text.
    canvas->DrawFocusRect(MirroredLeftPointForRect(r) - kFocusPaddingHorizontal,
                          r.y() - kFocusPaddingVertical,
                          r.width() + kFocusPaddingHorizontal * 2,
                          r.height() + kFocusPaddingVertical * 2);
  }
  PaintFloatingView(canvas, label_, r.x(), r.y(), r.width(), r.height());
}

void CheckBox::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  NativeButton::SetEnabled(enabled);
  label_->SetEnabled(enabled);
}

HWND CheckBox::CreateNativeControl(HWND parent_container) {
  HWND r = ::CreateWindowEx(WS_EX_TRANSPARENT | GetAdditionalExStyle(),
                            L"BUTTON",
                            L"",
                            WS_CHILD | BS_CHECKBOX | WS_VISIBLE,
                            0, 0, width(), height(),
                            parent_container, NULL, NULL, NULL);
  ConfigureNativeButton(r);
  return r;
}

void CheckBox::ConfigureNativeButton(HWND hwnd) {
  ::SendMessage(hwnd,
                static_cast<UINT>(BM_SETCHECK),
                static_cast<WPARAM>(is_selected_ ? BST_CHECKED : BST_UNCHECKED),
                0);
  label_->SetText(GetLabel());
}

gfx::Size CheckBox::GetPreferredSize() {
  gfx::Size prefsize = label_->GetPreferredSize();
  prefsize.set_height(std::max(prefsize.height() + kFocusPaddingVertical * 2,
                               kCheckBoxHeight));
  prefsize.Enlarge(GetTextIndent() * 2, 0);
  return prefsize;
}

LRESULT CheckBox::OnCommand(UINT code, int id, HWND source) {
  if (code == BN_CLICKED)
    SetIsSelected(!is_selected_);

  return NativeButton::OnCommand(code, id, source);
}

void CheckBox::HighlightButton(bool f) {
  ::SendMessage(GetNativeControlHWND(),
                static_cast<UINT>(BM_SETSTATE),
                static_cast<WPARAM>(f),
                0);
}

bool CheckBox::LabelHitTest(const MouseEvent& event) {
  CPoint p(event.x(), event.y());
  gfx::Rect r;
  ComputeTextRect(&r);
  return r.Contains(event.x(), event.y());
}

void CheckBox::OnMouseEntered(const MouseEvent& event) {
  HighlightButton(LabelHitTest(event));
}

void CheckBox::OnMouseMoved(const MouseEvent& event) {
  HighlightButton(LabelHitTest(event));
}

void CheckBox::OnMouseExited(const MouseEvent& event) {
  HighlightButton(false);
}

bool CheckBox::OnMousePressed(const MouseEvent& event) {
  HighlightButton(LabelHitTest(event));
  return true;
}

bool CheckBox::OnMouseDragged(const MouseEvent& event) {
  HighlightButton(LabelHitTest(event));
  return true;
}

void CheckBox::OnMouseReleased(const MouseEvent& event,
                               bool canceled) {
  HighlightButton(false);
  if (!canceled && LabelHitTest(event))
    OnCommand(BN_CLICKED, 0, GetNativeControlHWND());
}

}  // namespace views
