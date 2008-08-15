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

#include "chrome/common/gfx/chrome_font.h"

#include <windows.h>
#include <math.h>

#include <algorithm>

#include "base/logging.h"
#include "base/win_util.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/common/l10n_util.h"

#include "generated_resources.h"

/*static*/
ChromeFont::HFontRef* ChromeFont::base_font_ref_;

// If the tmWeight field of a TEXTMETRIC structure has a value >= this, the
// font is bold.
static const int kTextMetricWeightBold = 700;

//
// ChromeFont
//

// static
ChromeFont ChromeFont::CreateFont(HFONT font) {
  DCHECK(font);
  LOGFONT font_info;
  GetObject(font, sizeof(LOGFONT), &font_info);
  return ChromeFont(CreateHFontRef(CreateFontIndirect(&font_info)));
}

ChromeFont ChromeFont::CreateFont(const std::wstring& font_name,
                                  int font_size) {
  HDC hdc = GetDC(NULL);
  long lf_height = -MulDiv(font_size, GetDeviceCaps(hdc, LOGPIXELSY), 72);
  ReleaseDC(NULL, hdc);
  HFONT hf = ::CreateFont(lf_height, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    font_name.c_str());
  return ChromeFont::CreateFont(hf);
}

// static
ChromeFont::HFontRef* ChromeFont::GetBaseFontRef() {
  if (base_font_ref_ == NULL) {
		NONCLIENTMETRICS metrics;
    win_util::GetNonClientMetrics(&metrics);

    // See comment in ChromeFont::DeriveFont() about font size.
    DCHECK_GE(abs(metrics.lfMessageFont.lfHeight), 5);
    HFONT font = CreateFontIndirect(&metrics.lfMessageFont);
    DLOG_ASSERT(font);
    base_font_ref_ = ChromeFont::CreateHFontRef(font);
    // base_font_ref_ is global, up the ref count so it's never deleted.
    base_font_ref_->AddRef();
  }
  return base_font_ref_;
}

std::wstring ChromeFont::FontName() {
  LOGFONT font_info;
  GetObject(hfont(), sizeof(LOGFONT), &font_info);
  return (std::wstring(font_info.lfFaceName));
}

int ChromeFont::FontSize() {
  LOGFONT font_info;
  GetObject(hfont(), sizeof(LOGFONT), &font_info);
  long lf_height = font_info.lfHeight;
  HDC hdc = GetDC(NULL);
  int device_caps = GetDeviceCaps(hdc, LOGPIXELSY);
  int font_size = 0;
  if (device_caps != 0) {
    float font_size_float = -static_cast<float>(lf_height)*72/device_caps;
    font_size = static_cast<int>(::ceil(font_size_float - 0.5));
  }
  ReleaseDC(NULL, hdc);
  return font_size;
}

ChromeFont::HFontRef::HFontRef(HFONT hfont,
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

ChromeFont::HFontRef::~HFontRef() {
  DeleteObject(hfont_);
}


ChromeFont ChromeFont::DeriveFont(int size_delta, int style) const {
  LOGFONT font_info;
  GetObject(hfont(), sizeof(LOGFONT), &font_info);
  // LOGFONT returns two types of font heights, negative is measured slightly
  // differently (character height, vs cell height).
  if (font_info.lfHeight < 0) {
    font_info.lfHeight -= size_delta;
  } else {
    font_info.lfHeight += size_delta;
  }
  // Even with "Small Fonts", the smallest readable font size is 5. It is easy
  // to create a non-drawing font and forget about the fact that text should be
  // drawn in the UI. This test ensures that the font will be readable.
  DCHECK_GE(abs(font_info.lfHeight), 5);
  font_info.lfUnderline = ((style & UNDERLINED) == UNDERLINED);
  font_info.lfItalic = ((style & ITALIC) == ITALIC);
  font_info.lfWeight = (style & BOLD) ? FW_BOLD : FW_NORMAL;

  if (style & WEB) {
    font_info.lfPitchAndFamily = FF_SWISS;
    wcscpy_s(font_info.lfFaceName,
             l10n_util::GetString(IDS_WEB_FONT_FAMILY).c_str());
  }

  HFONT hfont = CreateFontIndirect(&font_info);
  return ChromeFont(CreateHFontRef(hfont));
}

int ChromeFont::GetStringWidth(const std::wstring& text) const {
  int width = 0;
  HDC dc = GetDC(NULL);
  HFONT previous_font = static_cast<HFONT>(SelectObject(dc, hfont()));
  SIZE size;
  if (GetTextExtentPoint32(dc, text.c_str(), static_cast<int>(text.size()),
                           &size)) {
    width = size.cx;
  } else {
    width = 0;
  }
  SelectObject(dc, previous_font);
  ReleaseDC(NULL, dc);
  return width;
}

ChromeFont::HFontRef* ChromeFont::CreateHFontRef(HFONT font) {
  TEXTMETRIC font_metrics;
  HDC screen_dc = GetDC(NULL);
  HFONT previous_font = static_cast<HFONT>(SelectObject(screen_dc, font));
  int last_map_mode = SetMapMode(screen_dc, MM_TEXT);
  GetTextMetrics(screen_dc, &font_metrics);
  // Yes, this is how Microsoft recommends calculating the dialog unit
  // conversions.
  SIZE ave_text_size;
  GetTextExtentPoint32(screen_dc,
                       L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
                       52, &ave_text_size);
  const int dlu_base_x = (ave_text_size.cx / 26 + 1) / 2;
  // To avoid the DC referencing font_handle_, select the previous font.
  SelectObject(screen_dc, previous_font);
  SetMapMode(screen_dc, last_map_mode);
  ReleaseDC(NULL, screen_dc);

  const int height = std::max(1, static_cast<int>(font_metrics.tmHeight));
  const int baseline = std::max(1, static_cast<int>(font_metrics.tmAscent));
  const int ave_char_width =
      std::max(1, static_cast<int>(font_metrics.tmAveCharWidth));
  int style = 0;
  if (font_metrics.tmItalic) {
    style |= ChromeFont::ITALIC;
  }
  if (font_metrics.tmUnderlined) {
    style |= ChromeFont::UNDERLINED;
  }
  if (font_metrics.tmWeight >= kTextMetricWeightBold) {
    style |= ChromeFont::BOLD;
  }

  return new HFontRef(font, height, baseline, ave_char_width, style,
                      dlu_base_x);
}

