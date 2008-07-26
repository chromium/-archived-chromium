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

#ifndef CHROME_VIEWS_TEXT_BUTTON_H__
#define CHROME_VIEWS_TEXT_BUTTON_H__

#include <windows.h>

#include "chrome/common/gfx/chrome_font.h"
#include "chrome/views/border.h"
#include "chrome/views/base_button.h"
#include "skia/include/SkBitmap.h"

namespace ChromeViews {

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

  void GetPreferredSize(CSize* result);
  void GetMinimumSize(CSize* result);
  virtual bool OnMousePressed(const ChromeViews::MouseEvent& e);

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
  CSize text_size_;

  // Track the size of the largest text string seen so far, so that
  // changing text_ will not resize the button boundary.
  CSize max_text_size_;

  TextAlignment alignment_;

  ChromeFont font_;

  // Text color.
  SkColor color_;

  SkBitmap icon_;

  // See setter for details.
  int max_width_;

  DISALLOW_EVIL_CONSTRUCTORS(TextButton);
};


} // namespace

#endif  // CHROME_VIEWS_TEXT_BUTTON_H__
