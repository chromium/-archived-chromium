// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_TEXT_BUTTON_H__
#define CHROME_VIEWS_TEXT_BUTTON_H__

#include <windows.h>

#include "chrome/common/gfx/chrome_font.h"
#include "chrome/views/border.h"
#include "chrome/views/base_button.h"
#include "skia/include/SkBitmap.h"

namespace views {

class MouseEvent;

////////////////////////////////////////////////////////////////////////////////
//
// TextButtonBorder
//
//  A Border subclass that paints a TextButton's background layer -
//  basically the button frame in the hot/pushed states.
//
////////////////////////////////////////////////////////////////////////////////
class TextButtonBorder : public Border {
 public:
  TextButtonBorder();
  virtual ~TextButtonBorder();

  // Render the background for the provided view
  virtual void Paint(const View& view, ChromeCanvas* canvas) const;

  // Returns the insets for the border.
  virtual void GetInsets(gfx::Insets* insets) const;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(TextButtonBorder);

  // Images
  struct MBBImageSet {
    SkBitmap* top_left;
    SkBitmap* top;
    SkBitmap* top_right;
    SkBitmap* left;
    SkBitmap* center;
    SkBitmap* right;
    SkBitmap* bottom_left;
    SkBitmap* bottom;
    SkBitmap* bottom_right;
  };
  MBBImageSet hot_set_;
  MBBImageSet pushed_set_;
};


////////////////////////////////////////////////////////////////////////////////
//
// TextButton
//
//  A button which displays text and/or and icon that can be changed in
//  response to actions. TextButton reserves space for the largest string
//  passed to SetText. To reset the cached max size invoke ClearMaxTextSize.
//
////////////////////////////////////////////////////////////////////////////////
class TextButton : public BaseButton {
public:
  TextButton(const std::wstring& text);
  virtual ~TextButton();

  typedef enum TextAlignment {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
  };

  virtual gfx::Size GetPreferredSize();
  virtual gfx::Size GetMinimumSize();
  virtual bool OnMousePressed(const MouseEvent& e);

  // Call SetText once per string in your set of possible values at
  // button creation time, so that it can contain the largest of them
  // and avoid resizing the button when the text changes.
  virtual void SetText(const std::wstring& text);

  void TextButton::SetTextAlignment(TextAlignment alignment) {
    alignment_ = alignment;
  }

  const std::wstring& GetText() { return text_; }

  // Sets the icon.
  void SetIcon(const SkBitmap& icon);

  const SkBitmap& GetIcon() { return icon_; }

  // TextButton remembers the maximum display size of the text passed to
  // SetText. This method resets the cached maximum display size to the
  // current size.
  void ClearMaxTextSize();

  virtual void Paint(ChromeCanvas* canvas);
  virtual void Paint(ChromeCanvas* canvas, bool for_drag);

  // Sets the enabled state. Setting the enabled state resets the color.
  virtual void SetEnabled(bool enabled);

  // Sets the max width. The preferred width of the button will never be larger
  // then the specified value. A value <= 0 indicates the preferred width
  // is not constrained in anyway.
  void set_max_width(int max_width) { max_width_ = max_width; }

 private:
  std::wstring text_;
  gfx::Size text_size_;

  // Track the size of the largest text string seen so far, so that
  // changing text_ will not resize the button boundary.
  gfx::Size max_text_size_;

  TextAlignment alignment_;

  ChromeFont font_;

  // Text color.
  SkColor color_;

  SkBitmap icon_;

  // See setter for details.
  int max_width_;

  DISALLOW_EVIL_CONSTRUCTORS(TextButton);
};


}  // namespace views

#endif  // CHROME_VIEWS_TEXT_BUTTON_H__
