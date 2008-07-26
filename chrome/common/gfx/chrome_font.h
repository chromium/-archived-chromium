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

#ifndef CHROME_COMMON_GFX_CHROME_FONT_H__
#define CHROME_COMMON_GFX_CHROME_FONT_H__

#include <windows.h>
#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"

// ChromeFont provides a wrapper around an underlying font. Copy and assignment
// operators are explicitly allowed, and cheap.
class ChromeFont {
 public:
  // The following constants indicate the font style.
  static const int BOLD = 1;
  static const int ITALIC = 2;
  static const int UNDERLINED = 4;
  static const int WEB = 8;

  // Creates a ChromeFont from the specified HFONT. The supplied HFONT is
  // effectively copied.
  static ChromeFont CreateFont(HFONT hfont);

  // Creates a ChromeFont given font name (e.g. arial), font size (e.g. 12).
  static ChromeFont CreateFont(const std::wstring& font_name, int font_size);

  // Creates a font with the default name and style.
  ChromeFont() : font_ref_(GetBaseFontRef()) { }
  ~ChromeFont() { }

  // Returns a new Font derived from the existing font.
  // size_deta is the size to add to the current font. For example, a value
  // of 5 results in a font 5 units bigger than this font.
  ChromeFont DeriveFont(int size_delta) const {
    return DeriveFont(size_delta, style());
  }

  // Returns a new Font derived from the existing font.
  // size_deta is the size to add to the current font. See the single
  // argument version of this method for an example.
  // The style parameter specifies the new style for the font, and is a
  // bitmask of the values: BOLD, ITALIC, UNDERLINED and WEB.
  ChromeFont DeriveFont(int size_delta, int style) const;

  // Returns the number of vertical pixels needed to display characters from
  // the specified font.
  int height() const { return font_ref_->height(); }

  // Returns the baseline, or ascent, of the font.
  int baseline() const { return font_ref_->baseline(); }

  // Returns the average character width for the font.
  int ave_char_width() const { return font_ref_->ave_char_width(); }

  // Returns the number of horizontal pixels needed to display the specified
  // string.
  int GetStringWidth(const std::wstring& text) const;

  // Returns the style of the font.
  int style() const { return font_ref_->style(); }

  // Returns the handle to the underlying HFONT. This is used by ChromeCanvas to
  // draw text.
  HFONT hfont() const { return font_ref_->hfont(); }

  // Dialog units to pixels conversion.
  // See http://support.microsoft.com/kb/145994 for details.
  int horizontal_dlus_to_pixels(int dlus) {
    return dlus * font_ref_->dlu_base_x() / 4;
  }
  int vertical_dlus_to_pixels(int dlus) {
    return dlus * font_ref_->height() / 8;
  }

  // Font Name.
  std::wstring FontName();

  // Font Size.
  int FontSize();

 private:
  // Chrome text drawing bottoms out in the Windows GDI functions that take an
  // HFONT (an opaque handle into Windows). To avoid lots of GDI object
  // allocation and destruction, ChromeFont indirectly refers to the HFONT
  // by way of an HFontRef. That is, every ChromeFront has an HFontRef, which
  // has an HFONT.
  //
  // HFontRef is reference counted. Upon deletion, it deletes the HFONT.
  // By making HFontRef maintain the reference to the HFONT, multiple
  // HFontRefs can share the same HFONT, and ChromeFont can provide value
  // semantics.
  class HFontRef : public base::RefCounted<HFontRef> {
   public:
    // This constructor takes control of the HFONT, and will delete it when
    // the HFontRef is deleted.
    HFontRef(HFONT hfont,
             int height,
             int baseline,
             int ave_char_width,
             int style,
             int dlu_base_x)
        : hfont_(hfont),
          height_(height),
          baseline_(baseline),
          ave_char_width_(ave_char_width),
          style_(style),
          dlu_base_x_(dlu_base_x) {
      DLOG_ASSERT(hfont);
    }

    ~HFontRef() {
      DeleteObject(hfont_);
    }

    // Accessors
    HFONT hfont() const { return hfont_; }
    int height() const { return height_; }
    int baseline() const { return baseline_; }
    int ave_char_width() const { return ave_char_width_; }
    int style() const { return style_; }
    int dlu_base_x() const { return dlu_base_x_; }

   private:
    const HFONT hfont_;
    const int height_;
    const int baseline_;
    const int ave_char_width_;
    const int style_;
    // Constants used in converting dialog units to pixels.
    const int dlu_base_x_;

    DISALLOW_EVIL_CONSTRUCTORS(HFontRef);
  };


  // Returns the base font ref. This should ONLY be invoked on the
  // UI thread.
  static HFontRef* GetBaseFontRef();

  // Creates and returns a new HFONTRef from the specified HFONT.
  static HFontRef* CreateHFontRef(HFONT font);

  explicit ChromeFont(HFontRef* font_ref) : font_ref_(font_ref) { }

  // Reference to the base font all fonts are derived from.
  static HFontRef* base_font_ref_;

  // Indirect reference to the HFontRef, which references the underlying HFONT.
  scoped_refptr<HFontRef> font_ref_;
};

#endif  // CHROME_COMMON_GFX_CHROME_FONT_H__
