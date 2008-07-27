/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "UniscribeStateTextRun.h"

#include "Font.h"
#include "SimpleFontData.h"

#include "webkit/glue/webkit_glue.h"

UniscribeStateTextRun::UniscribeStateTextRun(const WebCore::TextRun& run,
                                             const WebCore::Font& font)
    : UniscribeState(run.characters(), run.length(), run.rtl(),
                     font.primaryFont()->platformData().hfont(),
                     font.primaryFont()->scriptCache(),
                     font.primaryFont()->scriptFontProperties()),
       font_(&font),
       font_index_(0) {
    set_directional_override(run.directionalOverride());
    set_letter_spacing(font.letterSpacing());
    set_space_width(font.spaceWidth());
    set_word_spacing(font.wordSpacing());
    set_ascent(font.primaryFont()->ascent()); 

    Init();

    // Padding is the amount to add to make justification happen. This
    // should be done after Init() so all the runs are already measured.
    if (run.padding() > 0)
      Justify(run.padding());
}

UniscribeStateTextRun::UniscribeStateTextRun(
    const wchar_t* input,
    int input_length,
    bool is_rtl,
    HFONT hfont,
    SCRIPT_CACHE* script_cache,
    SCRIPT_FONTPROPERTIES* font_properties)
    : UniscribeState(input, input_length, is_rtl, hfont,
                     script_cache, font_properties),
      font_(NULL),
      font_index_(-1) {
}

void UniscribeStateTextRun::TryToPreloadFont(HFONT font) {
  // Ask the browser to get the font metrics for this font.
  // That will preload the font and it should now be accessible
  // from the renderer.
  webkit_glue::EnsureFontLoaded(font);
}

bool UniscribeStateTextRun::NextWinFontData(
    HFONT* hfont,
    SCRIPT_CACHE** script_cache,
    SCRIPT_FONTPROPERTIES** font_properties,
    int* ascent) {
  // This check is necessary because NextWinFontData can be
  // called again after we already ran out of fonts. fontDataAt
  // behaves in a strange manner when the difference between
  // param passed and # of fonts stored in WebKit::Font is
  // larger than one. We can avoid this check by setting 
  // font_index_ to # of elements in hfonts_ when we run out 
  // of font. In that case, we'd have to go through a couple of 
  // more checks before returning false.
  if (font_index_ == -1 || !font_)
    return false;

  // If the font data for a fallback font requested is not
  // yet retrieved, add them to our vectors. Note that '>' rather 
  // than '>=' is used to test that condition. primaryFont is not
  // stored in hfonts_, and friends so that indices for fontDataAt
  // and our vectors for font data are 1 off from each other. 
  // That is, when fully populated, hfonts_ and friends have
  // one font fewer than what's contained in font_. 
  if (static_cast<size_t>(++font_index_) > hfonts_->size()) {
    const WebCore::FontData *font_data;
    font_data = font_->fontDataAt(font_index_); 
    if (!font_data) {
      // run out of fonts
      font_index_ = -1;
      return false;
    }

    // TODO(ericroman): this won't work for SegmentedFontData
    // http://b/issue?id=1007335
    const WebCore::SimpleFontData* simple_font_data =
        font_data->fontDataForCharacter(' ');

    hfonts_->push_back(simple_font_data->platformData().hfont()); 
    script_caches_->push_back(simple_font_data->scriptCache());
    font_properties_->push_back(simple_font_data->scriptFontProperties());
    ascents_->push_back(simple_font_data->ascent()); 
  }

  *hfont = hfonts_[font_index_ - 1]; 
  *script_cache = script_caches_[font_index_ - 1];
  *font_properties = font_properties_[font_index_ - 1];
  *ascent = ascents_[font_index_ - 1];
  return true; 
}

void UniscribeStateTextRun::ResetFontIndex() {
  font_index_ = 0;
}
