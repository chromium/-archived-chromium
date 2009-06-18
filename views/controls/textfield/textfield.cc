// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/textfield/textfield.h"

#include "app/gfx/insets.h"
#if defined(OS_WIN)
#include "app/win_util.h"
#endif
#include "base/string_util.h"
#include "views/controls/textfield/native_textfield_wrapper.h"
#include "views/widget/widget.h"

namespace views {

// static
const char Textfield::kViewClassName[] = "views/Textfield";

/////////////////////////////////////////////////////////////////////////////
// Textfield

Textfield::Textfield()
    : native_wrapper_(NULL),
      controller_(NULL),
      style_(STYLE_DEFAULT),
      read_only_(false),
      default_width_in_chars_(0),
      draw_border_(true),
      background_color_(SK_ColorWHITE),
      use_default_background_color_(true),
      num_lines_(1),
      initialized_(false) {
  SetFocusable(true);
}

Textfield::Textfield(StyleFlags style)
    : native_wrapper_(NULL),
      controller_(NULL),
      style_(style),
      read_only_(false),
      default_width_in_chars_(0),
      draw_border_(true),
      background_color_(SK_ColorWHITE),
      use_default_background_color_(true),
      num_lines_(1),
      initialized_(false) {
  SetFocusable(true);
}

Textfield::~Textfield() {
  if (native_wrapper_)
    delete native_wrapper_;
}

void Textfield::SetController(Controller* controller) {
  controller_ = controller;
}

Textfield::Controller* Textfield::GetController() const {
  return controller_;
}

void Textfield::SetReadOnly(bool read_only) {
  read_only_ = read_only;
  if (native_wrapper_) {
    native_wrapper_->UpdateReadOnly();
    native_wrapper_->UpdateBackgroundColor();
  }
}

bool Textfield::IsPassword() const {
  return style_ & STYLE_PASSWORD;
}

bool Textfield::IsMultiLine() const {
  return !!(style_ & STYLE_MULTILINE);
}

void Textfield::SetText(const std::wstring& text) {
  text_ = text;
  if (native_wrapper_)
    native_wrapper_->UpdateText();
}

void Textfield::AppendText(const std::wstring& text) {
  text_ += text;
  if (native_wrapper_)
    native_wrapper_->AppendText(text);
}

void Textfield::SelectAll() {
  if (native_wrapper_)
    native_wrapper_->SelectAll();
}

void Textfield::ClearSelection() const {
  if (native_wrapper_)
    native_wrapper_->ClearSelection();
}

void Textfield::SetBackgroundColor(SkColor color) {
  background_color_ = color;
  use_default_background_color_ = false;
  if (native_wrapper_)
    native_wrapper_->UpdateBackgroundColor();
}

void Textfield::UseDefaultBackgroundColor() {
  use_default_background_color_ = true;
  if (native_wrapper_)
    native_wrapper_->UpdateBackgroundColor();
}

void Textfield::SetFont(const gfx::Font& font) {
  font_ = font;
  if (native_wrapper_)
    native_wrapper_->UpdateFont();
}

void Textfield::SetHorizontalMargins(int left, int right) {
  if (native_wrapper_)
    native_wrapper_->SetHorizontalMargins(left, right);
}

void Textfield::SetHeightInLines(int num_lines) {
  DCHECK(IsMultiLine());
  num_lines_ = num_lines;
}

void Textfield::RemoveBorder() {
  if (!draw_border_)
    return;

  draw_border_ = false;
  if (native_wrapper_)
    native_wrapper_->UpdateBorder();
}


void Textfield::CalculateInsets(gfx::Insets* insets) {
  DCHECK(insets);

  if (!draw_border_)
    return;

  // NOTE: One would think GetThemeMargins would return the insets we should
  // use, but it doesn't. The margins returned by GetThemeMargins are always
  // 0.

  // This appears to be the insets used by Windows.
  insets->Set(3, 3, 3, 3);
}

void Textfield::SyncText() {
  if (native_wrapper_)
    text_ = native_wrapper_->GetText();
}

// static
bool Textfield::IsKeystrokeEnter(const Keystroke& key) {
#if defined(OS_WIN)
  return key.key == VK_RETURN;
#else
  // TODO(port): figure out VK_constants
  NOTIMPLEMENTED();
  return false;
#endif
}

// static
bool Textfield::IsKeystrokeEscape(const Keystroke& key) {
#if defined(OS_WIN)
  return key.key == VK_ESCAPE;
#else
  // TODO(port): figure out VK_constants
  NOTIMPLEMENTED();
  return false;
#endif
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, View overrides:

void Textfield::Layout() {
  if (native_wrapper_) {
    native_wrapper_->GetView()->SetBounds(GetLocalBounds(true));
    native_wrapper_->GetView()->Layout();
  }
}

gfx::Size Textfield::GetPreferredSize() {
  gfx::Insets insets;
  CalculateInsets(&insets);
  return gfx::Size(font_.GetExpectedTextWidth(default_width_in_chars_) +
                       insets.width(),
                   num_lines_ * font_.height() + insets.height());
}

bool Textfield::IsFocusable() const {
  return IsEnabled() && !read_only_;
}

void Textfield::AboutToRequestFocusFromTabTraversal(bool reverse) {
  SelectAll();
}

bool Textfield::SkipDefaultKeyEventProcessing(const KeyEvent& e) {
#if defined(OS_WIN)
  // TODO(hamaji): Figure out which keyboard combinations we need to add here,
  //               similar to LocationBarView::SkipDefaultKeyEventProcessing.
  const int c = e.GetCharacter();
  if (c == VK_BACK)
    return true;  // We'll handle BackSpace ourselves.

  // We don't translate accelerators for ALT + NumPad digit, they are used for
  // entering special characters.  We do translate alt-home.
  if (e.IsAltDown() && (c != VK_HOME) &&
      win_util::IsNumPadDigit(c, e.IsExtendedKey()))
    return true;
#endif
  return false;
}

void Textfield::SetEnabled(bool enabled) {
  View::SetEnabled(enabled);
  if (native_wrapper_)
    native_wrapper_->UpdateEnabled();
}

void Textfield::Focus() {
  if (native_wrapper_) {
    // Forward the focus to the wrapper if it exists.
    native_wrapper_->SetFocus();
  } else {
    // If there is no wrapper, cause the RootView to be focused so that we still
    // get keyboard messages.
    View::Focus();
  }
}

void Textfield::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (is_add && !native_wrapper_ && GetWidget() && !initialized_) {
    initialized_ = true;
    native_wrapper_ = NativeTextfieldWrapper::CreateWrapper(this);
    //AddChildView(native_wrapper_->GetView());
    // TODO(beng): Move this initialization to NativeTextfieldWin once it
    //             subclasses NativeControlWin.
    native_wrapper_->UpdateText();
    native_wrapper_->UpdateBackgroundColor();
    native_wrapper_->UpdateReadOnly();
    native_wrapper_->UpdateFont();
    native_wrapper_->UpdateEnabled();
    native_wrapper_->UpdateBorder();

    // We need to call Layout here because any previous calls to Layout
    // will have short-circuited and we don't call AddChildView.
    Layout();
  }
}

std::string Textfield::GetClassName() const {
  return kViewClassName;
}

}  // namespace views
