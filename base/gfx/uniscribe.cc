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

#include <windows.h>

#include "base/gfx/uniscribe.h"

#include "base/gfx/font_utils.h"
#include "base/logging.h"

namespace gfx {

// This function is used to see where word spacing should be applied inside
// runs. Note that this must match Font::treatAsSpace so we all agree where
// and how much space this is, so we don't want to do more general Unicode
// "is this a word break" thing.
static bool TreatAsSpace(wchar_t c) {
 return c == ' ' || c == '\t' || c == '\n' || c == 0x00A0;
}

// SCRIPT_FONTPROPERTIES contains glyph indices for default, invalid
// and blank glyphs. Just because ScriptShape succeeds does not mean
// that a text run is rendered correctly. Some characters may be rendered
// with default/invalid/blank glyphs. Therefore, we need to check if the glyph
// array returned by ScriptShape contains any of those glyphs to make
// sure that the text run is rendered successfully.
static bool ContainsMissingGlyphs(WORD *glyphs,
                                  int length,
                                  SCRIPT_FONTPROPERTIES* properties) {
  for (int i = 0; i < length; ++i) {
    if (glyphs[i] == properties->wgDefault ||
        (glyphs[i] == properties->wgInvalid && glyphs[i] != properties->wgBlank))
      return true;
  }

  return false;
}

// HFONT is the 'incarnation' of 'everything' about font, but it's an opaque
// handle and we can't directly query it to make a new HFONT sharing
// its characteristics (height, style, etc) except for family name.
// This function uses GetObject to convert HFONT back to LOGFONT,
// resets the fields of LOGFONT and calculates style to use later
// for the creation of a font identical to HFONT other than family name.
static void SetLogFontAndStyle(HFONT hfont, LOGFONT *logfont, int *style) {
  DCHECK(hfont && logfont);
  if (!hfont || !logfont)
    return;

  GetObject(hfont, sizeof(LOGFONT), logfont);
  // We reset these fields to values appropriate for CreateFontIndirect.
  // while keeping lfHeight, which is the most important value in creating
  // a new font similar to hfont.
  logfont->lfWidth = 0;
  logfont->lfEscapement = 0;
  logfont->lfOrientation = 0;
  logfont->lfCharSet = DEFAULT_CHARSET;
  logfont->lfOutPrecision = OUT_TT_ONLY_PRECIS;
  logfont->lfQuality = DEFAULT_QUALITY;  // Honor user's desktop settings.
  logfont->lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
  if (style)
    *style = gfx::GetStyleFromLogfont(logfont);
}

UniscribeState::UniscribeState(const wchar_t* input,
                               int input_length,
                               bool is_rtl,
                               HFONT hfont,
                               SCRIPT_CACHE* script_cache,
                               SCRIPT_FONTPROPERTIES* font_properties)
    : input_(input),
      input_length_(input_length),
      is_rtl_(is_rtl),
      hfont_(hfont),
      script_cache_(script_cache),
      font_properties_(font_properties),
      directional_override_(false),
      inhibit_ligate_(false),
      letter_spacing_(0),
      space_width_(0),
      word_spacing_(0),
      ascent_(0) {
  logfont_.lfFaceName[0] = 0;
}

UniscribeState::~UniscribeState() {
}

void UniscribeState::InitWithOptionalLengthProtection(bool length_protection) {
  // We cap the input length and just don't do anything. We'll allocate a lot
  // of things of the size of the number of characters, so the allocated memory
  // will be several times the input length. Plus shaping such a large buffer
  // may be a form of denial of service. No legitimate text should be this long.
  // It also appears that Uniscribe flatly rejects very long strings, so we
  // don't lose anything by doing this.
  //
  // The input length protection may be disabled by the unit tests to cause
  // an error condition.
  static const int kMaxInputLength = 65535;
  if (input_length_ == 0 ||
      (length_protection && input_length_ > kMaxInputLength))
    return;

  FillRuns();
  FillShapes();
  FillScreenOrder();
}

int UniscribeState::Width() const {
  int width = 0;
  for (int item_index = 0; item_index < static_cast<int>(runs_->size());
       item_index++) {
    width += AdvanceForItem(item_index);
  }
  return width;
}

void UniscribeState::Justify(int additional_space) {
  // Count the total number of glyphs we have so we know how big to make the
  // buffers below.
  int total_glyphs = 0;
  for (size_t run = 0; run < runs_->size(); run++) {
    int run_idx = screen_order_[run];
    total_glyphs += static_cast<int>(shapes_[run_idx].glyph_length());
  }
  if (total_glyphs == 0)
    return;  // Nothing to do.

  // We make one big buffer in screen order of all the glyphs we are drawing
  // across runs so that the justification function will adjust evenly across
  // all glyphs.
  StackVector<SCRIPT_VISATTR, 64> visattr;
  visattr->resize(total_glyphs);
  StackVector<int, 64> advances;
  advances->resize(total_glyphs);
  StackVector<int, 64> justify;
  justify->resize(total_glyphs);

  // Build the packed input.
  int dest_index = 0;
  for (size_t run = 0; run < runs_->size(); run++) {
    int run_idx = screen_order_[run];
    const Shaping& shaping = shapes_[run_idx];

    for (int i = 0; i < shaping.glyph_length(); i++, dest_index++) {
      memcpy(&visattr[dest_index], &shaping.visattr[i], sizeof(SCRIPT_VISATTR));
      advances[dest_index] = shaping.advance[i];
    }
  }

  // The documentation for ScriptJustify is wrong, the parameter is the space
  // to add and not the width of the column you want.
  const int min_kashida = 1;  // How do we decide what this should be?
  ScriptJustify(&visattr[0], &advances[0], total_glyphs, additional_space,
                min_kashida, &justify[0]);

  // Now we have to unpack the justification amounts back into the runs so
  // the glyph indices match.
  int global_glyph_index = 0;
  for (size_t run = 0; run < runs_->size(); run++) {
    int run_idx = screen_order_[run];
    Shaping& shaping = shapes_[run_idx];

    shaping.justify->resize(shaping.glyph_length());
    for (int i = 0; i < shaping.glyph_length(); i++, global_glyph_index++)
      shaping.justify[i] = justify[global_glyph_index];
  }
}

int UniscribeState::CharacterToX(int offset) const {
  HRESULT hr;
  DCHECK(offset <= input_length_);

  // Our algorithm is to traverse the items in screen order from left to
  // right, adding in each item's screen width until we find the item with
  // the requested character in it.
  int width = 0;
  for (size_t screen_idx = 0; screen_idx < runs_->size(); screen_idx++) {
    // Compute the length of this run.
    int item_idx = screen_order_[screen_idx];
    const SCRIPT_ITEM& item = runs_[item_idx];
    const Shaping& shaping = shapes_[item_idx];
    int item_length = shaping.char_length();

    if (offset >= item.iCharPos && offset <= item.iCharPos + item_length) {
      // Character offset is in this run.
      int char_len = offset - item.iCharPos;

      int cur_x = 0;
      hr = ScriptCPtoX(char_len, FALSE, item_length, shaping.glyph_length(),
                       &shaping.logs[0], &shaping.visattr[0],
                       shaping.effective_advances(), &item.a, &cur_x);
      if (FAILED(hr))
        return 0;

      width += cur_x + shaping.pre_padding;
      DCHECK(width >= 0);
      return width;
    }

    // Move to the next item.
    width += AdvanceForItem(item_idx);
  }
  DCHECK(width >= 0);
  return width;
}

int UniscribeState::XToCharacter(int x) const {
  // We iterate in screen order until we find the item with the given pixel
  // position in it. When we find that guy, we ask Uniscribe for the
  // character index.
  HRESULT hr;
  for (size_t screen_idx = 0; screen_idx < runs_->size(); screen_idx++) {
    int item_idx = screen_order_[screen_idx];
    int advance_for_item = AdvanceForItem(item_idx);

    // Note that the run may be empty if shaping failed, so we want to skip
    // over it.
    const Shaping& shaping = shapes_[item_idx];
    int item_length = shaping.char_length();
    if (x <= advance_for_item && item_length > 0) {
      // The requested offset is within this item.
      const SCRIPT_ITEM& item = runs_[item_idx];

      // Account for the leading space we've added to this run that Uniscribe
      // doesn't know about.
      x -= shaping.pre_padding;

      int char_x = 0;
      int trailing;
      hr = ScriptXtoCP(x, item_length, shaping.glyph_length(),
                       &shaping.logs[0], &shaping.visattr[0],
                       shaping.effective_advances(), &item.a, &char_x,
                       &trailing);

      // The character offset is within the item. We need to add the item's
      // offset to transform it into the space of the TextRun
      return char_x + item.iCharPos;
    }

    // The offset is beyond this item, account for its length and move on.
    x -= advance_for_item;
  }

  // Error condition, we don't know what to do if we don't have that X
  // position in any of our items.
  return 0;
}

void UniscribeState::Draw(HDC dc, int x, int y, int from, int to) {
  HGDIOBJ old_font = 0;
  int cur_x = x;
  bool first_run = true;

  for (size_t screen_idx = 0; screen_idx < runs_->size(); screen_idx++) {
    int item_idx = screen_order_[screen_idx];
    const SCRIPT_ITEM& item = runs_[item_idx];
    const Shaping& shaping = shapes_[item_idx];

    // Character offsets within this run. THESE MAY NOT BE IN RANGE and may
    // be negative, etc. The code below handles this.
    int from_char = from - item.iCharPos;
    int to_char = to - item.iCharPos;

    // See if we need to draw any characters in this item.
    if (shaping.char_length() == 0 ||
        from_char >= shaping.char_length() || to_char <= 0) {
      // No chars in this item to display.
      cur_x += AdvanceForItem(item_idx);
      continue;
    }

    // Compute the starting glyph within this span. |from| and |to| are
    // global offsets that may intersect arbitrarily with our local run.
    int from_glyph, after_glyph;
    if (item.a.fRTL) {
      // To compute the first glyph when going RTL, we use |to|.
      if (to_char >= shaping.char_length()) {
        // The end of the text is after (to the left) of us.
        from_glyph = 0;
      } else {
        // Since |to| is exclusive, the first character we draw on the left
        // is actually the one right before (to the right) of |to|.
        from_glyph = shaping.logs[to_char - 1];
      }

      // The last glyph is actually the first character in the range.
      if (from_char <= 0) {
        // The first character to draw is before (to the right) of this span,
        // so draw all the way to the end.
        after_glyph = shaping.glyph_length();
      } else {
        // We want to draw everything up until the character to the right of
        // |from|. To the right is - 1, so we look that up (remember our
        // character could be more than one glyph, so we can't look up our
        // glyph and add one).
        after_glyph = shaping.logs[from_char - 1];
      }
    } else {
      // Easy case, everybody agrees about directions. We only need to handle
      // boundary conditions to get a range inclusive at the beginning, and
      // exclusive at the ending. We have to do some computation to see the
      // glyph one past the end.
      from_glyph = shaping.logs[from_char < 0 ? 0 : from_char];
      if (to_char >= shaping.char_length())
        after_glyph = shaping.glyph_length();
      else
        after_glyph = shaping.logs[to_char];
    }

    // Account for the characters that were skipped in this run. When
    // WebKit asks us to draw a subset of the run, it actually tells us
    // to draw at the X offset of the beginning of the run, since it
    // doesn't know the internal position of any of our characters.
    const int* effective_advances = shaping.effective_advances();
    int inner_offset = 0;
    for (int i = 0; i < from_glyph; i++)
      inner_offset += effective_advances[i];

    // Actually draw the glyphs we found.
    int glyph_count = after_glyph - from_glyph;
    if (from_glyph >= 0 && glyph_count > 0) {
      // Account for the preceeding space we need to add to this run. We don't
      // need to count for the following space because that will be counted
      // in AdvanceForItem below when we move to the next run.
      inner_offset += shaping.pre_padding;

      // Pass NULL in when there is no justification.
      const int* justify = shaping.justify->empty() ?
          NULL : &shaping.justify[from_glyph];

      if (first_run) {
        old_font = SelectObject(dc, shaping.hfont_);
        first_run = false;
      } else {
        SelectObject(dc, shaping.hfont_);
      }

      // TODO(brettw) bug 698452: if a half a character is selected,
      // we should set up a clip rect so we draw the half of the glyph
      // correctly.
      // Fonts with different ascents can be used to render different runs.
      // 'Across-runs' y-coordinate correction needs to be adjusted
      // for each font.
      HRESULT hr = S_FALSE;
      for (int executions = 0; executions < 2; ++executions) {
        hr = ScriptTextOut(dc, shaping.script_cache_, cur_x + inner_offset,
                           y - shaping.ascent_offset_, 0, NULL, &item.a, NULL,
                           0, &shaping.glyphs[from_glyph],
                           glyph_count, &shaping.advance[from_glyph],
                           justify, &shaping.offsets[from_glyph]);
        if (S_OK != hr && 0 == executions) {
          // If this ScriptTextOut is called from the renderer it might fail
          // because the sandbox is preventing it from opening the font files.
          // If we are running in the renderer, TryToPreloadFont is overridden
          // to ask the browser to preload the font for us so we can access it.
          TryToPreloadFont(shaping.hfont_);
          continue;
        }
        break;
      }

      DCHECK(S_OK == hr);


    }

    cur_x += AdvanceForItem(item_idx);
  }

  if (old_font)
    SelectObject(dc, old_font);
}

WORD UniscribeState::FirstGlyphForCharacter(int char_offset) const {
  // Find the run for the given character.
  for (int i = 0; i < static_cast<int>(runs_->size()); i++) {
    int first_char = runs_[i].iCharPos;
    const Shaping& shaping = shapes_[i];
    int local_offset = char_offset - first_char;
    if (local_offset >= 0 && local_offset < shaping.char_length()) {
      // The character is in this run, return the first glyph for it (should
      // generally be the only glyph). It seems Uniscribe gives glyph 0 for
      // empty, which is what we want to return in the "missing" case.
      size_t glyph_index = shaping.logs[local_offset];
      if (glyph_index >= shaping.glyphs->size()) {
        // The glyph should be in this run, but the run has too few actual
        // characters. This can happen when shaping the run fails, in which
        // case, we should have no data in the logs at all.
        DCHECK(shaping.glyphs->empty());
        return 0;
      }
      return shaping.glyphs[glyph_index];
    }
  }
  return 0;
}

void UniscribeState::FillRuns() {
  HRESULT hr;
  runs_->resize(UNISCRIBE_STATE_STACK_RUNS);

  SCRIPT_STATE input_state;
  input_state.uBidiLevel = is_rtl_;
  input_state.fOverrideDirection = directional_override_;
  input_state.fInhibitSymSwap = false;
  input_state.fCharShape = false;  // Not implemented in Uniscribe
  input_state.fDigitSubstitute = false;  // Do we want this for Arabic?
  input_state.fInhibitLigate = inhibit_ligate_;
  input_state.fDisplayZWG = false;  // Don't draw control characters.
  input_state.fArabicNumContext = is_rtl_;  // Do we want this for Arabic?
  input_state.fGcpClusters = false;
  input_state.fReserved = 0;
  input_state.fEngineReserved = 0;
  // The psControl argument to ScriptItemize should be non-NULL for RTL text,
  // per http://msdn.microsoft.com/en-us/library/ms776532.aspx . So use a
  // SCRIPT_CONTROL that is set to all zeros.  Zero as a locale ID means the
  // neutral locale per http://msdn.microsoft.com/en-us/library/ms776294.aspx .
  static SCRIPT_CONTROL input_control = {0, // uDefaultLanguage    :16;
                                         0, // fContextDigits      :1;
                                         0, // fInvertPreBoundDir  :1;
                                         0, // fInvertPostBoundDir :1;
                                         0, // fLinkStringBefore   :1;
                                         0, // fLinkStringAfter    :1;
                                         0, // fNeutralOverride    :1;
                                         0, // fNumericOverride    :1;
                                         0, // fLegacyBidiClass    :1;
                                         0, // fMergeNeutralItems  :1;
                                         0};// fReserved           :7;
  // Calling ScriptApplyDigitSubstitution( NULL, &input_control, &input_state)
  // here would be appropriate if we wanted to set the language ID, and get
  // local digit substitution behavior.  For now, don't do it.

  while (true) {
    int num_items = 0;

    // Ideally, we would have a way to know the runs before and after this
    // one, and put them into the control parameter of ScriptItemize. This
    // would allow us to shape characters properly that cross style
    // boundaries (WebKit bug 6148).
    //
    // We tell ScriptItemize that the output list of items is one smaller
    // than it actually is. According to Mozilla bug 366643, if there is
    // not enough room in the array on pre-SP2 systems, ScriptItemize will
    // write one past the end of the buffer.
    //
    // ScriptItemize is very strange. It will often require a much larger
    // ITEM buffer internally than it will give us as output. For example,
    // it will say a 16-item buffer is not big enough, and will write
    // interesting numbers into all those items. But when we give it a 32
    // item buffer and it succeeds, it only has one item output.
    //
    // It seems to be doing at least two passes, the first where it puts a
    // lot of intermediate data into our items, and the second where it
    // collates them.
    hr = ScriptItemize(input_, input_length_,
                       static_cast<int>(runs_->size()) - 1, &input_control, &input_state,
                       &runs_[0], &num_items);
    if (SUCCEEDED(hr)) {
      runs_->resize(num_items);
      break;
    }
    if (hr != E_OUTOFMEMORY) {
      // Some kind of unexpected error.
      runs_->resize(0);
      break;
    }
    // There was not enough items for it to write into, expand.
    runs_->resize(runs_->size() * 2);
  }

  // Fix up the directions of the items so they're what WebKit thinks
  // they are. WebKit (and we assume any other caller) always knows what
  // direction it wants things to be in, and will only give us runs that are in
  // the same direction. Sometimes, Uniscibe disagrees, for example, if you
  // have embedded ASCII punctuation in an Arabic string, WebKit will
  // (correctly) know that is should still be rendered RTL, but Uniscibe might
  // think LTR is better.
  //
  // TODO(brettw) bug 747235:
  // This workaround fixes the bug but causes spacing problems in other cases.
  // WebKit sometimes gives us a big run that includes ASCII and Arabic, and
  // this forcing direction makes those cases incorrect. This seems to happen
  // during layout only, so it ends up that spacing is incorrect (because being
  // the wrong direction changes ligatures and stuff).
  //
  //for (size_t i = 0; i < runs_->size(); i++)
  //  runs_[i].a.fRTL = is_rtl_;
}


bool UniscribeState::Shape(const wchar_t* input,
                           int item_length,
                           int num_glyphs,
                           SCRIPT_ITEM& run,
                           Shaping& shaping) {
  HFONT hfont = hfont_;
  SCRIPT_CACHE* script_cache = script_cache_;
  SCRIPT_FONTPROPERTIES* font_properties = font_properties_;
  int ascent = ascent_;
  HDC temp_dc = NULL;
  HGDIOBJ old_font = 0;
  HRESULT hr;
  bool lastFallbackTried = false;
  bool result;

  int generated_glyphs = 0;

  // In case HFONT passed in ctor cannot render this run, we have to scan
  // other fonts from the beginning of the font list.
  ResetFontIndex();

  // Compute shapes.
  while (true) {
    shaping.logs->resize(item_length);
    shaping.glyphs->resize(num_glyphs);
    shaping.visattr->resize(num_glyphs);

    // Firefox sets SCRIPT_ANALYSIS.SCRIPT_STATE.fDisplayZWG to true
    // here. Is that what we want? It will display control characters.
    hr = ScriptShape(temp_dc, script_cache, input, item_length,
                     num_glyphs, &run.a,
                     &shaping.glyphs[0], &shaping.logs[0],
                     &shaping.visattr[0], &generated_glyphs);
    if (hr == E_PENDING) {
      // Allocate the DC.
      temp_dc = GetDC(NULL);
      old_font = SelectObject(temp_dc, hfont);
      continue;
    } else if (hr == E_OUTOFMEMORY) {
      num_glyphs *= 2;
      continue;
    } else if (SUCCEEDED(hr) &&
               (lastFallbackTried || !ContainsMissingGlyphs(&shaping.glyphs[0],
                generated_glyphs, font_properties))) {
      break;
    }

    // The current font can't render this run. clear DC and try
    // next font.
    if (temp_dc) {
      SelectObject(temp_dc, old_font);
      ReleaseDC(NULL, temp_dc);
      temp_dc = NULL;
    }

    if (NextWinFontData(&hfont, &script_cache, &font_properties, &ascent)) {
      // The primary font does not support this run. Try next font.
      // In case of web page rendering, they come from fonts specified in
      // CSS stylesheets.
      continue;
    } else if (!lastFallbackTried) {
      lastFallbackTried = true;

      // Generate a last fallback font based on the script of
      // a character to draw while inheriting size and styles
      // from the primary font
      if (!logfont_.lfFaceName[0])
        SetLogFontAndStyle(hfont_, &logfont_, &style_);

      // TODO(jungshik): generic type should come from webkit for
      // UniscribeStateTextRun (a derived class used in webkit).
      const wchar_t *family = GetFallbackFamily(input, item_length,
          GENERIC_FAMILY_STANDARD);
      bool font_ok = GetDerivedFontData(family, style_, &logfont_, &ascent, &hfont, &script_cache);

      if (!font_ok) {
        // If this GetDerivedFontData is called from the renderer it might fail
        // because the sandbox is preventing it from opening the font files.
        // If we are running in the renderer, TryToPreloadFont is overridden to
        // ask the browser to preload the font for us so we can access it.
        TryToPreloadFont(hfont);

        // Try again.
        font_ok = GetDerivedFontData(family, style_, &logfont_, &ascent, &hfont, &script_cache);
        DCHECK(font_ok);
      }

      // TODO(jungshik) : Currently GetDerivedHFont always returns a
      // a valid HFONT, but in the future, I may change it to return 0.
      DCHECK(hfont);

      // We don't need a font_properties for the last resort fallback font
      // because we don't have anything more to try and are forced to
      // accept empty glyph boxes. If we tried a series of fonts as
      // 'last-resort fallback', we'd need it, but currently, we don't.
      continue;
    } else if (hr == USP_E_SCRIPT_NOT_IN_FONT) {
      run.a.eScript = SCRIPT_UNDEFINED;
      continue;
    } else if (FAILED(hr)) {
      // Error shaping.
      generated_glyphs = 0;
      result = false;
      goto cleanup;
    }
  }

  // Sets Windows font data for this run to those corresponding to
  // a font supporting this run. we don't need to store font_properties
  // because it's not used elsewhere.
  shaping.hfont_ = hfont;
  shaping.script_cache_ = script_cache;

  // The ascent of a font for this run can be different from
  // that of the primary font so that we need to keep track of
  // the difference per run and take that into account when calling
  // ScriptTextOut in |Draw|. Otherwise, different runs rendered by
  // different fonts would not be aligned vertically.
  shaping.ascent_offset_ = ascent_ ? ascent - ascent_ : 0;
  result = true;

cleanup:
  shaping.glyphs->resize(generated_glyphs);
  shaping.visattr->resize(generated_glyphs);
  shaping.advance->resize(generated_glyphs);
  shaping.offsets->resize(generated_glyphs);
  if (temp_dc) {
    SelectObject(temp_dc, old_font);
    ReleaseDC(NULL, temp_dc);
  }
  // On failure, our logs don't mean anything, so zero those out.
  if (!result)
    shaping.logs->clear();

  return result;
}

void UniscribeState::FillShapes() {
  shapes_->resize(runs_->size());
  for (size_t i = 0; i < runs_->size(); i++) {
    int start_item = runs_[i].iCharPos;
    int item_length = input_length_ - start_item;
    if (i < runs_->size() - 1)
      item_length = runs_[i + 1].iCharPos - start_item;

    int num_glyphs;
    if (item_length < UNISCRIBE_STATE_STACK_CHARS) {
      // We'll start our buffer sizes with the current stack space available
      // in our buffers if the current input fits. As long as it
      // doesn't expand past that we'll save a lot of time mallocing.
      num_glyphs = UNISCRIBE_STATE_STACK_CHARS;
    } else {
      // When the input doesn't fit, give up with the stack since it will
      // almost surely not be enough room (unless the input actually shrinks,
      // which is unlikely) and just start with the length recommended by
      // the Uniscribe documentation as a "usually fits" size.
      num_glyphs = item_length * 3 / 2 + 16;
    }

    // Convert a string to a glyph string trying the primary font,
    // fonts in the fallback list and then script-specific last resort font.
    Shaping& shaping = shapes_[i];
    if (!Shape(&input_[start_item], item_length, num_glyphs, runs_[i], shaping))
      continue;

    // Compute placements. Note that offsets is documented incorrectly
    // and is actually an array.

    // DC that we lazily create if Uniscribe commands us to.
    // (this does not happen often because script_cache is already
    //  updated when calling ScriptShape).
    HDC temp_dc = NULL;
    HGDIOBJ old_font = NULL;
    HRESULT hr;
    while (true) {
      shaping.pre_padding = 0;
      hr = ScriptPlace(temp_dc, shaping.script_cache_, &shaping.glyphs[0],
                       static_cast<int>(shaping.glyphs->size()),
                       &shaping.visattr[0], &runs_[i].a,
                       &shaping.advance[0], &shaping.offsets[0],
                       &shaping.abc);
      if (hr != E_PENDING)
        break;

      // Allocate the DC and run the loop again.
      temp_dc = GetDC(NULL);
      old_font = SelectObject(temp_dc, shaping.hfont_);
    }

    if (FAILED(hr)) {
      // Some error we don't know how to handle. Nuke all of our data
      // since we can't deal with partially valid data later.
      runs_->clear();
      shapes_->clear();
      screen_order_->clear();
    }

    if (temp_dc) {
      SelectObject(temp_dc, old_font);
      ReleaseDC(NULL, temp_dc);
    }
  }

  AdjustSpaceAdvances();

  if (letter_spacing_ != 0 || word_spacing_ != 0)
    ApplySpacing();
}

void UniscribeState::FillScreenOrder() {
  screen_order_->resize(runs_->size());

  // We assume that the input has only one text direction in it.
  // TODO(brettw) are we sure we want to keep this restriction?
  if (is_rtl_) {
    for (int i = 0; i < static_cast<int>(screen_order_->size()); i++)
      screen_order_[static_cast<int>(screen_order_->size()) - i - 1] = i;
  } else {
    for (int i = 0; i < static_cast<int>(screen_order_->size()); i++)
      screen_order_[i] = i;
  }
}

void UniscribeState::AdjustSpaceAdvances() {
  if (space_width_ == 0)
    return;

  int space_width_without_letter_spacing = space_width_ - letter_spacing_;

  // This mostly matches what WebKit's UniscribeController::shapeAndPlaceItem.
  for (size_t run = 0; run < runs_->size(); run++) {
    Shaping& shaping = shapes_[run];

    for (int i = 0; i < shaping.char_length(); i++) {
      if (!TreatAsSpace(input_[runs_[run].iCharPos + i]))
        continue;

      int glyph_index = shaping.logs[i];
      int current_advance = shaping.advance[glyph_index];
      // Don't give zero-width spaces a width.
      if (!current_advance)
        continue;

      // current_advance does not include additional letter-spacing, but
      // space_width does. Here we find out how off we are from the correct
      // width for the space not including letter-spacing, then just subtract
      // that diff.
      int diff = current_advance - space_width_without_letter_spacing;
      // The shaping can consist of a run of text, so only subtract the
      // difference in the width of the glyph.
      shaping.advance[glyph_index] -= diff;
      shaping.abc.abcB -= diff;
    }
  }
}

void UniscribeState::ApplySpacing() {
  for (size_t run = 0; run < runs_->size(); run++) {
    Shaping& shaping = shapes_[run];
    bool is_rtl = runs_[run].a.fRTL;

    if (letter_spacing_ != 0) {
      // RTL text gets padded to the left of each character. We increment the
      // run's advance to make this happen. This will be balanced out by NOT
      // adding additional advance to the last glyph in the run.
      if (is_rtl)
        shaping.pre_padding += letter_spacing_;

      // Go through all the glyphs in this run and increase the "advance" to
      // account for letter spacing. We adjust letter spacing only on cluster
      // boundaries.
      //
      // This works for most scripts, but may have problems with some indic
      // scripts. This behavior is better than Firefox or IE for Hebrew.
      for (int i = 0; i < shaping.glyph_length(); i++) {
        if (shaping.visattr[i].fClusterStart) {
          // Ick, we need to assign the extra space so that the glyph comes
          // first, then is followed by the space. This is opposite for RTL.
          if (is_rtl) {
            if (i != shaping.glyph_length() - 1) {
              // All but the last character just get the spacing applied to
              // their advance. The last character doesn't get anything,
              shaping.advance[i] += letter_spacing_;
              shaping.abc.abcB += letter_spacing_;
            }
          } else {
            // LTR case is easier, we just add to the advance.
            shaping.advance[i] += letter_spacing_;
            shaping.abc.abcB += letter_spacing_;
          }
        }
      }
    }

    // Go through all the characters to find whitespace and insert the extra
    // wordspacing amount for the glyphs they correspond to.
    if (word_spacing_ != 0) {
      for (int i = 0; i < shaping.char_length(); i++) {
        if (!TreatAsSpace(input_[runs_[run].iCharPos + i]))
          continue;

        // The char in question is a word separator...
        int glyph_index = shaping.logs[i];

        // Spaces will not have a glyph in Uniscribe, it will just add
        // additional advance to the character to the left of the space. The
        // space's corresponding glyph will be the character following it in
        // reading order.
        if (is_rtl) {
          // In RTL, the glyph to the left of the space is the same as the
          // first glyph of the following character, so we can just increment
          // it.
          shaping.advance[glyph_index] += word_spacing_;
          shaping.abc.abcB += word_spacing_;
        } else {
          // LTR is actually more complex here, we apply it to the previous
          // character if there is one, otherwise we have to apply it to the
          // leading space of the run.
          if (glyph_index == 0) {
            shaping.pre_padding += word_spacing_;
          } else {
            shaping.advance[glyph_index - 1] += word_spacing_;
            shaping.abc.abcB += word_spacing_;
          }
        }
      }
    }  // word_spacing_ != 0

    // Loop for next run...
  }
}

// The advance is the ABC width of the run
int UniscribeState::AdvanceForItem(int item_index) const {
  int accum = 0;
  const Shaping& shaping = shapes_[item_index];

  if (shaping.justify->empty()) {
    // Easy case with no justification, the width is just the ABC width of	t		
    // the run. (The ABC width is the sum of the advances).
    return shaping.abc.abcA + shaping.abc.abcB + shaping.abc.abcC +
        shaping.pre_padding;
  }

  // With justification, we use the justified amounts instead. The
  // justification array contains both the advance and the extra space
  // added for justification, so is the width we want.
  int justification = 0;
  for (size_t i = 0; i < shaping.justify->size(); i++)
    justification += shaping.justify[i];

  return shaping.pre_padding + justification;
}

}  // namespace gfx
