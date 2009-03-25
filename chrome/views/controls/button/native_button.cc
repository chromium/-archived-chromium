// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/views/controls/button/native_button.h"

#include "base/logging.h"
#include "chrome/common/l10n_util.h"

namespace views {

static int kButtonBorderHWidth = 8;

// static
const char NativeButton::kViewClassName[] = "chrome/views/NativeButton";

////////////////////////////////////////////////////////////////////////////////
// NativeButton, public:

NativeButton::NativeButton(ButtonListener* listener)
    : Button(listener),
      native_wrapper_(NULL),
      is_default_(false),
      ignore_minimum_size_(false),
      minimum_size_(50, 14) {
  // The min size in DLUs comes from
  // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dnwue/html/ch14e.asp
  InitBorder();
  SetFocusable(true);
}

NativeButton::NativeButton(ButtonListener* listener, const std::wstring& label)
    : Button(listener),
      native_wrapper_(NULL),
      label_(label),
      is_default_(false),
      ignore_minimum_size_(false),
      minimum_size_(50, 14) {
  // The min size in DLUs comes from
  // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dnwue/html/ch14e.asp
  InitBorder();
  SetFocusable(true);
}

NativeButton::~NativeButton() {
}

void NativeButton::SetLabel(const std::wstring& label) {
  label_ = label;

  // Even though we create a flipped HWND for a native button when the locale
  // is right-to-left, Windows does not render text for the button using a
  // right-to-left context (perhaps because the parent HWND is not flipped).
  // The result is that RTL strings containing punctuation marks are not
  // displayed properly. For example, the string "...ABC" (where A, B and C are
  // Hebrew characters) is displayed as "ABC..." which is incorrect.
  //
  // In order to overcome this problem, we mark the localized Hebrew strings as
  // RTL strings explicitly (using the appropriate Unicode formatting) so that
  // Windows displays the text correctly regardless of the HWND hierarchy.
  std::wstring localized_label;
  if (l10n_util::AdjustStringForLocaleDirection(label_, &localized_label))
    label_ = localized_label;

  if (native_wrapper_)
    native_wrapper_->UpdateLabel();
}

void NativeButton::SetIsDefault(bool is_default) {
  if (is_default == is_default_)
    return;
  is_default_ = is_default;
  if (native_wrapper_)
    native_wrapper_->UpdateDefault();
}

void NativeButton::ButtonPressed() {
  RequestFocus();

  // TODO(beng): obtain mouse event flags for native buttons someday.
  NotifyClick(mouse_event_flags());
}

////////////////////////////////////////////////////////////////////////////////
// NativeButton, View overrides:

gfx::Size NativeButton::GetPreferredSize() {
  if (!native_wrapper_)
    return gfx::Size();

  gfx::Size sz = native_wrapper_->GetView()->GetPreferredSize();

  // Add in the border size. (Do this before clamping the minimum size in case
  // that clamping causes an increase in size that would include the borders.
  gfx::Insets border = GetInsets();
  sz.set_width(sz.width() + border.left() + border.right());
  sz.set_height(sz.height() + border.top() + border.bottom());

  // Clamp the size returned to at least the minimum size.
  if (!ignore_minimum_size_) {
    if (minimum_size_.width()) {
      int min_width = font_.horizontal_dlus_to_pixels(minimum_size_.width());
      sz.set_width(std::max(static_cast<int>(sz.width()), min_width));
    }
    if (minimum_size_.height()) {
      int min_height = font_.vertical_dlus_to_pixels(minimum_size_.height());
      sz.set_height(std::max(static_cast<int>(sz.height()), min_height));
    }
  }

  return sz;
}

void NativeButton::Layout() {
  if (native_wrapper_) {
    native_wrapper_->GetView()->SetBounds(0, 0, width(), height());
    native_wrapper_->GetView()->Layout();
  }
}

void NativeButton::SetEnabled(bool flag) {
  Button::SetEnabled(flag);
  if (native_wrapper_)
    native_wrapper_->UpdateEnabled();
}

void NativeButton::ViewHierarchyChanged(bool is_add, View* parent,
                                         View* child) {
  if (is_add && !native_wrapper_ && GetWidget()) {
    CreateWrapper();
    AddChildView(native_wrapper_->GetView());
  }
}

std::string NativeButton::GetClassName() const {
  return kViewClassName;
}

bool NativeButton::AcceleratorPressed(const Accelerator& accelerator) {
  if (IsEnabled()) {
    NotifyClick(mouse_event_flags());
    return true;
  }
  return false;
}

void NativeButton::Focus() {
  Button::Focus();
  // Forward the focus to the wrapper.
  if (native_wrapper_)
    native_wrapper_->SetFocus();
}

////////////////////////////////////////////////////////////////////////////////
// NativeButton, protected:

void NativeButton::CreateWrapper() {
  native_wrapper_ = NativeButtonWrapper::CreateNativeButtonWrapper(this);
  native_wrapper_->UpdateLabel();
  native_wrapper_->UpdateEnabled();
}

void NativeButton::InitBorder() {
  set_border(Border::CreateEmptyBorder(0, kButtonBorderHWidth, 0,
                                       kButtonBorderHWidth));
}

}  // namespace views
