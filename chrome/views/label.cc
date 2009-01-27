// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/label.h"

#include <math.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/insets.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/background.h"

namespace views {

const char Label::kViewClassName[] = "chrome/views/Label";

static const SkColor kEnabledColor = SK_ColorBLACK;
static const SkColor kDisabledColor = SkColorSetRGB(161, 161, 146);

Label::Label() {
  Init(L"", GetDefaultFont());
}

Label::Label(const std::wstring& text) {
  Init(text, GetDefaultFont());
}

Label::Label(const std::wstring& text, const ChromeFont& font) {
  Init(text, font);
}

void Label::Init(const std::wstring& text, const ChromeFont& font) {
  contains_mouse_ = false;
  font_ = font;
  text_size_valid_ = false;
  SetText(text);
  url_set_ = false;
  color_ = kEnabledColor;
  horiz_alignment_ = ALIGN_CENTER;
  is_multi_line_ = false;
}

Label::~Label() {
}

gfx::Size Label::GetPreferredSize() {
  gfx::Size prefsize;
  if (is_multi_line_) {
    int w = width(), h = 0;
    ChromeCanvas::SizeStringInt(text_, font_, &w, &h, ComputeMultiLineFlags());
    prefsize.SetSize(w, h);
  } else {
    prefsize = GetTextSize();
  }

  gfx::Insets insets = GetInsets();
  prefsize.Enlarge(insets.width(), insets.height());
  return prefsize;
}

int Label::ComputeMultiLineFlags() {
  int flags = ChromeCanvas::MULTI_LINE;
  switch (horiz_alignment_) {
    case ALIGN_LEFT:
      flags |= ChromeCanvas::TEXT_ALIGN_LEFT;
      break;
    case ALIGN_CENTER:
      flags |= ChromeCanvas::TEXT_ALIGN_CENTER;
      break;
    case ALIGN_RIGHT:
      flags |= ChromeCanvas::TEXT_ALIGN_RIGHT;
      break;
  }
  return flags;
}

void Label::CalculateDrawStringParams(
    std::wstring* paint_text, gfx::Rect* text_bounds, int* flags) {
  DCHECK(paint_text && text_bounds && flags);

  if (url_set_) {
    // TODO(jungshik) : Figure out how to get 'intl.accept_languages'
    // preference and use it when calling ElideUrl.
    *paint_text = gfx::ElideUrl(url_, font_, width(), std::wstring());

    // An URLs is always treated as an LTR text and therefore we should
    // explicitly mark it as such if the locale is RTL so that URLs containing
    // Hebrew or Arabic characters are displayed correctly.
    //
    // Note that we don't check the View's UI layout setting in order to
    // determine whether or not to insert the special Unicode formatting
    // characters. We use the locale settings because an URL is always treated
    // as an LTR string, even if its containing view does not use an RTL UI
    // layout.
    if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
      l10n_util::WrapStringWithLTRFormatting(paint_text);
  } else {
    *paint_text = text_;
  }

  if (is_multi_line_) {
    gfx::Insets insets = GetInsets();
    text_bounds->SetRect(insets.left(),
                         insets.top(),
                         width() - insets.width(),
                         height() - insets.height());
    *flags = ComputeMultiLineFlags();
  } else {
    *text_bounds = GetTextBounds();
    *flags = 0;
  }
}

void Label::Paint(ChromeCanvas* canvas) {
  PaintBackground(canvas);
  std::wstring paint_text;
  gfx::Rect text_bounds;
  int flags = 0;
  CalculateDrawStringParams(&paint_text, &text_bounds, &flags);
  canvas->DrawStringInt(paint_text,
                        font_,
                        color_,
                        text_bounds.x(),
                        text_bounds.y(),
                        text_bounds.width(),
                        text_bounds.height(),
                        flags);

  if (is_multi_line_) {
    PaintFocusBorder(canvas);
  } else {
    // We'll draw the focus border ourselves, so it is around the text.
    if (HasFocus())
      canvas->DrawFocusRect(text_bounds.x(),
                            text_bounds.y(),
                            text_bounds.width(),
                            text_bounds.height());
  }
}

void Label::PaintBackground(ChromeCanvas* canvas) {
  const Background* bg = contains_mouse_ ? GetMouseOverBackground() : NULL;
  if (!bg)
    bg = background();
  if (bg)
    bg->Paint(canvas, this);
}

void Label::SetFont(const ChromeFont& font) {
  font_ = font;
  text_size_valid_ = false;
  SchedulePaint();
}

ChromeFont Label::GetFont() const {
  return font_;
}

void Label::SetText(const std::wstring& text) {
  text_ = text;
  url_set_ = false;
  text_size_valid_ = false;
  SchedulePaint();
}

void Label::SetURL(const GURL& url) {
  url_ = url;
  text_ = UTF8ToWide(url_.spec());
  url_set_ = true;
  text_size_valid_ = false;
  SchedulePaint();
}

const std::wstring Label::GetText() const {
  if (url_set_)
    return UTF8ToWide(url_.spec());
  else
    return text_;
}

const GURL Label::GetURL() const {
  if (url_set_)
    return url_;
  else
    return GURL(WideToUTF8(text_));
}

gfx::Size Label::GetTextSize() {
  if (!text_size_valid_) {
    text_size_.SetSize(font_.GetStringWidth(text_), font_.height());
    text_size_valid_ = true;
  }

  if (text_size_valid_)
    return text_size_;
  return gfx::Size();
}

int Label::GetHeightForWidth(int w) {
  if (is_multi_line_) {
    gfx::Insets insets = GetInsets();
    w = std::max<int>(0, w - insets.width());
    int h = 0;
    ChromeCanvas cc(0, 0, true);
    cc.SizeStringInt(text_, font_, &w, &h, ComputeMultiLineFlags());
    return h + insets.height();
  }

  return View::GetHeightForWidth(w);
}

std::string Label::GetClassName() const {
  return kViewClassName;
}

void Label::SetColor(const SkColor& color) {
  color_ = color;
}

const SkColor Label::GetColor() const {
  return color_;
}

void Label::SetHorizontalAlignment(Alignment a) {
  if (horiz_alignment_ != a) {

    // If the View's UI layout is right-to-left, we need to flip the alignment
    // so that the alignment settings take into account the text
    // directionality.
    if (UILayoutIsRightToLeft()) {
      if (a == ALIGN_LEFT)
        a = ALIGN_RIGHT;
      else if (a == ALIGN_RIGHT)
        a = ALIGN_LEFT;
    }
    horiz_alignment_ = a;
    SchedulePaint();
  }
}

Label::Alignment Label::GetHorizontalAlignment() const {
  return horiz_alignment_;
}

void Label::SetMultiLine(bool f) {
  if (f != is_multi_line_) {
    is_multi_line_ = f;
    SchedulePaint();
  }
}

bool Label::IsMultiLine() {
  return is_multi_line_;
}

void Label::SetTooltipText(const std::wstring& tooltip_text) {
  tooltip_text_ = tooltip_text;
}

bool Label::GetTooltipText(int x, int y, std::wstring* tooltip) {
  DCHECK(tooltip);

  // If a tooltip has been explicitly set, use it.
  if (!tooltip_text_.empty()) {
    tooltip->assign(tooltip_text_);
    return true;
  }

  // Show the full text if the text does not fit.
  if (!is_multi_line_ && font_.GetStringWidth(text_) > width()) {
    *tooltip = text_;
    return true;
  }
  return false;
}

void Label::OnMouseMoved(const MouseEvent& e) {
  UpdateContainsMouse(e);
}

void Label::OnMouseEntered(const MouseEvent& event) {
  UpdateContainsMouse(event);
}

void Label::OnMouseExited(const MouseEvent& event) {
  SetContainsMouse(false);
}

void Label::SetMouseOverBackground(Background* background) {
  mouse_over_background_.reset(background);
}

const Background* Label::GetMouseOverBackground() const {
  return mouse_over_background_.get();
}

void Label::SetEnabled(bool enabled) {
  if (enabled == enabled_)
    return;
  View::SetEnabled(enabled);
  SetColor(enabled ? kEnabledColor : kDisabledColor);
}

// static
ChromeFont Label::GetDefaultFont() {
  return ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);
}

void Label::UpdateContainsMouse(const MouseEvent& event) {
  SetContainsMouse(GetTextBounds().Contains(event.x(), event.y()));
}

void Label::SetContainsMouse(bool contains_mouse) {
  if (contains_mouse_ == contains_mouse)
    return;
  contains_mouse_ = contains_mouse;
  if (GetMouseOverBackground())
    SchedulePaint();
}

gfx::Rect Label::GetTextBounds() {
  gfx::Size text_size = GetTextSize();
  gfx::Insets insets = GetInsets();
  int avail_width = width() - insets.width();
  // Respect the size set by the owner view
  text_size.set_width(std::min(avail_width, text_size.width()));

  int text_y = insets.top() +
      (height() - text_size.height() - insets.height()) / 2;
  int text_x;
  switch (horiz_alignment_) {
    case ALIGN_LEFT:
      text_x = insets.left();
      break;
    case ALIGN_CENTER:
      // We put any extra margin pixel on the left rather than the right, since
      // GetTextExtentPoint32() can report a value one too large on the right.
      text_x = insets.left() + (avail_width + 1 - text_size.width()) / 2;
      break;
    case ALIGN_RIGHT:
      text_x = width() - insets.right() - text_size.width();
      break;
  }
  return gfx::Rect(text_x, text_y, text_size.width(), text_size.height());
}

void Label::SizeToFit(int max_width) {
  DCHECK(is_multi_line_);

  std::vector<std::wstring> lines;
  SplitString(text_, L'\n', &lines);

  int label_width = 0;
  for (std::vector<std::wstring>::const_iterator iter = lines.begin();
       iter != lines.end(); ++iter) {
    label_width = std::max(label_width, font_.GetStringWidth(*iter));
  }

  gfx::Insets insets = GetInsets();
  label_width += insets.width();

  if (max_width > 0)
    label_width = std::min(label_width, max_width);

  SetBounds(x(), y(), label_width, 0);
  SizeToPreferredSize();
}

#if defined(OS_WIN)
bool Label::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);

  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_TEXT;
  return true;
}

bool Label::GetAccessibleName(std::wstring* name) {
  *name = GetText();
  return true;
}

bool Label::GetAccessibleState(VARIANT* state) {
  DCHECK(state);

  state->lVal |= STATE_SYSTEM_READONLY;
  return true;
}
#endif  // defined(OS_WIN)

}  // namespace views
