// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GFX_FONT_H_
#define APP_GFX_FONT_H_

#include "build/build_config.h"

#include <string>

#if defined(OS_WIN)
typedef struct HFONT__* HFONT;
#elif defined(OS_LINUX)
#include "third_party/skia/include/core/SkRefCnt.h"
class SkPaint;
class SkTypeface;
#endif

#if defined(OS_WIN)
typedef struct HFONT__* NativeFont;
#elif defined(OS_MACOSX)
#ifdef __OBJC__
@class NSFont;
#else
class NSFont;
#endif
typedef NSFont* NativeFont;
#elif defined(OS_LINUX)
class SkTypeface;
typedef SkTypeface* NativeFont;
#else  // null port.
#error No known OS defined
#endif

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

namespace gfx {

// Font provides a wrapper around an underlying font. Copy and assignment
// operators are explicitly allowed, and cheap.
class Font {
 public:
  // The following constants indicate the font style.
  enum {
    NORMAL = 0,
    BOLD = 1,
    ITALIC = 2,
    UNDERLINED = 4,
  };

  // Creates a Font given font name (e.g. arial), font size (e.g. 12).
  // Skia actually expects a family name and not a font name.
  static Font CreateFont(const std::wstring& font_name, int font_size);

  ~Font() { }

  // Returns a new Font derived from the existing font.
  // size_deta is the size to add to the current font. For example, a value
  // of 5 results in a font 5 units bigger than this font.
  Font DeriveFont(int size_delta) const {
    return DeriveFont(size_delta, style());
  }

  // Returns a new Font derived from the existing font.
  // size_delta is the size to add to the current font. See the single
  // argument version of this method for an example.
  // The style parameter specifies the new style for the font, and is a
  // bitmask of the values: BOLD, ITALIC and UNDERLINED.
  Font DeriveFont(int size_delta, int style) const;

  // Returns the number of vertical pixels needed to display characters from
  // the specified font.
  int height() const;

  // Returns the baseline, or ascent, of the font.
  int baseline() const;

  // Returns the average character width for the font.
  int ave_char_width() const;

  // Returns the number of horizontal pixels needed to display the specified
  // string.
  int GetStringWidth(const std::wstring& text) const;

  // Returns the expected number of horizontal pixels needed to display
  // the specified length of characters.
  // Call GetStringWidth() to retrieve the actual number.
  int GetExpectedTextWidth(int length) const;

  // Returns the style of the font.
  int style() const;

  // Font Name.
  // It is actually a font family name, because Skia expects a family name
  // and not a font name.
  std::wstring FontName();

  // Font Size.
  int FontSize();

  NativeFont nativeFont() const;

  // Creates a font with the default name and style.
  Font();

#if defined(OS_WIN)
  // Creates a Font from the specified HFONT. The supplied HFONT is effectively
  // copied.
  static Font CreateFont(HFONT hfont);

  // Returns the handle to the underlying HFONT. This is used by gfx::Canvas to
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
#elif defined(OS_LINUX)
  // We need a copy constructor and assignment operator to deal with
  // the Skia reference counting.
  Font(const Font& other);
  Font& operator=(const Font& other);
  // Setup a Skia context to use the current typeface
  void PaintSetup(SkPaint* paint) const;
#endif

 private:

#if defined(OS_WIN)
  // Chrome text drawing bottoms out in the Windows GDI functions that take an
  // HFONT (an opaque handle into Windows). To avoid lots of GDI object
  // allocation and destruction, Font indirectly refers to the HFONT by way of
  // an HFontRef. That is, every Font has an HFontRef, which has an HFONT.
  //
  // HFontRef is reference counted. Upon deletion, it deletes the HFONT.
  // By making HFontRef maintain the reference to the HFONT, multiple
  // HFontRefs can share the same HFONT, and Font can provide value semantics.
  class HFontRef : public base::RefCounted<HFontRef> {
   public:
    // This constructor takes control of the HFONT, and will delete it when
    // the HFontRef is deleted.
    HFontRef(HFONT hfont,
             int height,
             int baseline,
             int ave_char_width,
             int style,
             int dlu_base_x);
    ~HFontRef();

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

    DISALLOW_COPY_AND_ASSIGN(HFontRef);
  };

  // Returns the base font ref. This should ONLY be invoked on the
  // UI thread.
  static HFontRef* GetBaseFontRef();

  // Creates and returns a new HFONTRef from the specified HFONT.
  static HFontRef* CreateHFontRef(HFONT font);

  explicit Font(HFontRef* font_ref) : font_ref_(font_ref) { }

  // Reference to the base font all fonts are derived from.
  static HFontRef* base_font_ref_;

  // Indirect reference to the HFontRef, which references the underlying HFONT.
  scoped_refptr<HFontRef> font_ref_;
#elif defined(OS_LINUX)
  explicit Font(SkTypeface* typeface, const std::wstring& name,
                int size, int style);
  // Calculate and cache the font metrics.
  void calculateMetrics();
  // Make |this| a copy of |other|.
  void CopyFont(const Font& other);

  // The default font, used for the default constructor.
  static Font* default_font_;

  // These two both point to the same SkTypeface. We use the SkAutoUnref to
  // handle the reference counting, but without @typeface_ we would have to
  // cast the SkRefCnt from @typeface_helper_ every time.
  scoped_ptr<SkAutoUnref> typeface_helper_;
  SkTypeface *typeface_;

  // Additional information about the face
  // Skia actually expects a family name and not a font name.
  std::wstring font_family_;
  int font_size_;
  int style_;

  // Cached metrics, generated at construction
  int height_;
  int ascent_;
  int avg_width_;
#elif defined(OS_MACOSX)
  explicit Font(const std::wstring& font_name, int font_size, int style);

  // Calculate and cache the font metrics.
  void calculateMetrics();

  std::wstring font_name_;
  int font_size_;
  int style_;

  // Cached metrics, generated at construction
  int height_;
  int ascent_;
  int avg_width_;
#endif

};

}  // namespace gfx

#endif  // APP_GFX_FONT_H_
