// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A wrapper around Uniscribe that provides a reasonable API.

#ifndef BASE_GFX_UNISCRIBE_H__
#define BASE_GFX_UNISCRIBE_H__

#include <windows.h>
#include <usp10.h>
#include <wchar.h>
#include <map>
#include <vector>

#include "base/stack_container.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace gfx {

#define UNISCRIBE_STATE_STACK_RUNS 8
#define UNISCRIBE_STATE_STACK_CHARS 32

// This object should be safe to create & destroy frequently, as long as the
// caller preserves the script_cache when possible (this data may be slow to
// compute).
//
// This object is "kind of large" (~1K) because it reserves a lot of space for
// working with to avoid expensive heap operations. Therefore, not only should
// you not worry about creating and destroying it, you should try to not keep
// them around.
class UniscribeState {
 public:
  // Initializes this Uniscribe run with the text pointed to by |run| with
  // |length|. The input is NOT null terminated.
  //
  // The is_rtl flag should be set if the input script is RTL. It is assumed
  // that the caller has already divided up the input text (using ICU, for
  // example) into runs of the same direction of script. This avoids
  // disagreements between the caller and Uniscribe later (see FillItems).
  //
  // A script cache should be provided by the caller that is initialized to
  // NULL. When the caller is done with the cache (it may be stored between
  // runs as long as it is used consistently with the same HFONT), it should
  // call ScriptFreeCache().
  UniscribeState(const wchar_t* input,
                 int input_length,
                 bool is_rtl,
                 HFONT hfont,
                 SCRIPT_CACHE* script_cache,
                 SCRIPT_FONTPROPERTIES* font_properties);

  virtual ~UniscribeState();

  // Sets Uniscribe's directional override flag. False by default.
  bool directional_override() const {
    return directional_override_;
  }
  void set_directional_override(bool override) {
    directional_override_ = override;
  }

  // Set's Uniscribe's no-ligate override flag. False by default.
  bool inhibit_ligate() const {
    return inhibit_ligate_;
  }
  void set_inhibit_ligate(bool inhibit) {
    inhibit_ligate_ = inhibit;
  }

  // Set letter spacing. We will try to insert this much space between
  // graphemes (one or more glyphs perceived as a single unit by ordinary users
  // of a script). Positive values increase letter spacing, negative values
  // decrease it. 0 by default.
  int letter_spacing() const {
    return letter_spacing_;
  }
  void set_letter_spacing(int letter_spacing) {
    letter_spacing_ = letter_spacing;
  }

  // Set the width of a standard space character. We use this to normalize
  // space widths. Windows will make spaces after Hindi characters larger than
  // other spaces. A space_width of 0 means to use the default space width.
  //
  // Must be set before Init() is called.
  int space_width() const {
    return space_width_;
  }
  void set_space_width(int space_width) {
    space_width_ = space_width;
  }

  // Set word spacing. We will try to insert this much extra space between
  // each word in the input (beyond whatever whitespace character separates
  // words). Positive values lead to increased letter spacing, negative values
  // decrease it. 0 by default.
  //
  // Must be set before Init() is called.
  int word_spacing() const {
    return word_spacing_;
  }
  void set_word_spacing(int word_spacing) {
    word_spacing_ = word_spacing;
  }
  void set_ascent(int ascent) {
    ascent_ = ascent;
  }

  // You must call this after setting any options but before doing any
  // other calls like asking for widths or drawing.
  void Init() { InitWithOptionalLengthProtection(true); }

  // Returns the total width in pixels of the text run.
  int Width() const;

  // Call to justify the text, with the amount of space that should be ADDED to
  // get the desired width that the column should be justified to. Normally,
  // spaces are inserted, but for Arabic there will be kashidas (extra strokes)
  // inserted instead.
  //
  // This function MUST be called AFTER Init().
  void Justify(int additional_space);

  // Computes the given character offset into a pixel offset of the beginning
  // of that character.
  int CharacterToX(int offset) const;

  // Converts the given pixel X position into a logical character offset into
  // the run. For positions appearing before the first character, this will
  // return -1.
  int XToCharacter(int x) const;

  // Draws the given characters to (x, y) in the given DC. The font will be
  // handled by this function, but the font color and other attributes should
  // be pre-set.
  //
  // The y position is the upper left corner, NOT the baseline.
  void Draw(HDC dc, int x, int y, int from, int to);

  // Returns the first glyph assigned to the character at the given offset.
  // This function is used to retrieve glyph information when Uniscribe is
  // being used to generate glyphs for non-complex, non-BMP (above U+FFFF)
  // characters. These characters are not otherwise special and have no
  // complex shaping rules, so we don't otherwise need Uniscribe, except
  // Uniscribe is the only way to get glyphs for non-BMP characters.
  //
  // Returns 0 if there is no glyph for the given character.
  WORD FirstGlyphForCharacter(int char_offset) const;

 protected:
  // Backend for init. The flag allows the unit test to specify whether we
  // should fail early for very long strings like normal, or try to pass the
  // long string to Uniscribe. The latter provides a way to force failure of
  // shaping.
  void InitWithOptionalLengthProtection(bool length_protection);

  // Tries to preload the font when the it is not accessible.
  // This is the default implementation and it does not do anything.
  virtual void TryToPreloadFont(HFONT font) {}

 private:
  FRIEND_TEST(UniscribeTest, TooBig);

  // An array corresponding to each item in runs_ containing information
  // on each of the glyphs that were generated. Like runs_, this is in
  // reading order. However, for rtl text, the characters within each
  // item will be reversed.
  struct Shaping {
    Shaping()
        : pre_padding(0),
          hfont_(NULL),
          script_cache_(NULL),
          ascent_offset_(0) {
      abc.abcA = 0;
      abc.abcB = 0;
      abc.abcC = 0;
    }

    // Returns the number of glyphs (which will be drawn to the screen)
    // in this run.
    int glyph_length() const {
      return static_cast<int>(glyphs->size());
    }

    // Returns the number of characters (that we started with) in this run.
    int char_length() const {
      return static_cast<int>(logs->size());
    }

    // Returns the advance array that should be used when measuring glyphs.
    // The returned pointer will indicate an array with glyph_length() elements
    // and the advance that should be used for each one. This is either the
    // real advance, or the justified advances if there is one, and is the
    // array we want to use for measurement.
    const int* effective_advances() const {
      if (advance->empty())
        return 0;
      if (justify->empty())
        return &advance[0];
      return &justify[0];
    }

    // This is the advance amount of space that we have added to the beginning
    // of the run. It is like the ABC's |A| advance but one that we create and
    // must handle internally whenever computing with pixel offsets.
    int pre_padding;

    // Glyph indices in the font used to display this item. These indices
    // are in screen order.
    StackVector<WORD, UNISCRIBE_STATE_STACK_CHARS> glyphs;

    // For each input character, this tells us the first glyph index it
    // generated. This is the only array with size of the input chars.
    //
    // All offsets are from the beginning of this run. Multiple characters can
    // generate one glyph, in which case there will be adjacent duplicates in
    // this list. One character can also generate multiple glyphs, in which
    // case there will be skipped indices in this list.
    StackVector<WORD, UNISCRIBE_STATE_STACK_CHARS> logs;

    // Flags and such for each glyph.
    StackVector<SCRIPT_VISATTR, UNISCRIBE_STATE_STACK_CHARS> visattr;

    // Horizontal advances for each glyph listed above, this is basically
    // how wide each glyph is.
    StackVector<int, UNISCRIBE_STATE_STACK_CHARS> advance;

    // This contains glyph offsets, from the nominal position of a glyph. It
    // is used to adjust the positions of multiple combining characters
    // around/above/below base characters in a context-sensitive manner so
    // that they don't bump against each other and the base character.
    StackVector<GOFFSET, UNISCRIBE_STATE_STACK_CHARS> offsets;

    // Filled by a call to Justify, this is empty for nonjustified text.
    // If nonempty, this contains the array of justify characters for each
    // character as returned by ScriptJustify.
    //
    // This is the same as the advance array, but with extra space added for
    // some characters. The difference between a glyph's |justify| width and
    // it's |advance| width is the extra space added.
    StackVector<int, UNISCRIBE_STATE_STACK_CHARS> justify;

    // Sizing information for this run. This treats the entire run as a
    // character with a preceeding advance, width, and ending advance.
    // The B width is the sum of the |advance| array, and the A and C widths
    // are any extra spacing applied to each end.
    //
    // It is unclear from the documentation what this actually means. From
    // experimentation, it seems that the sum of the character advances is
    // always the sum of the ABC values, and I'm not sure what you're supposed
    // to do with the ABC values.
    ABC abc;

    // Pointers to windows font data used to render this run.
    HFONT hfont_;
    SCRIPT_CACHE* script_cache_;

    // Ascent offset between the ascent of the primary font
    // and that of the fallback font. The offset needs to be applied,
    // when drawing a string, to align multiple runs rendered with
    // different fonts.
    int ascent_offset_;
  };

  // Computes the runs_ array from the text run.
  void FillRuns();

  // Computes the shapes_ array given an runs_ array already filled in.
  void FillShapes();

  // Fills in the screen_order_ array (see below).
  void FillScreenOrder();

  // Called to update the glyph positions based on the current spacing options
  // that are set.
  void ApplySpacing();

  // Normalizes all advances for spaces to the same width. This keeps windows
  // from making spaces after Hindi characters larger, which is then
  // inconsistent with our meaure of the width since WebKit doesn't include
  // spaces in text-runs sent to uniscribe unless white-space:pre.
  void AdjustSpaceAdvances();

  // Returns the total width of a single item.
  int AdvanceForItem(int item_index) const;

  // Shapes a run (pointed to by |input|) using |hfont| first.
  // Tries a series of fonts specified retrieved with NextWinFontData
  // and finally a font covering characters in |*input|. A string pointed
  // by |input| comes from ScriptItemize and is supposed to contain
  // characters belonging to a single script aside from characters
  // common to all scripts (e.g. space).
  bool Shape(const wchar_t* input,
             int item_length,
             int num_glyphs,
             SCRIPT_ITEM& run,
             Shaping& shaping);

  // Gets Windows font data for the next best font to try in the list
  // of fonts. When there's no more font available, returns false
  // without touching any of out params. Need to call ResetFontIndex
  // to start scanning of the font list from the beginning.
  virtual bool NextWinFontData(HFONT* hfont,
                               SCRIPT_CACHE** script_cache,
                               SCRIPT_FONTPROPERTIES** font_properties,
                               int* ascent) {
    return false;
  }

  // Resets the font index to the first in the list of fonts
  // to try after the primaryFont turns out not to work. With font_index
  // reset, NextWinFontData scans fallback fonts from the beginning.
  virtual void ResetFontIndex() {}

  // The input data for this run of Uniscribe. See the constructor.
  const wchar_t* input_;
  const int input_length_;
  const bool is_rtl_;

  // Windows font data for the primary font :
  // In a sense, logfont_ and style_ are redundant because
  // hfont_ contains all the information. However, invoking GetObject,
  // everytime we need the height and the style, is rather expensive so
  // that we cache them. Would it be better to add getter and (virtual)
  // setter for the height and the style of the primary font, instead of
  // logfont_? Then, a derived class ctor can set ascent_, height_ and style_
  // if they're known. Getters for them would have to 'infer' their values from
  // hfont_ ONLY when they're not set.
  HFONT hfont_;
  SCRIPT_CACHE* script_cache_;
  SCRIPT_FONTPROPERTIES* font_properties_;
  int ascent_;
  LOGFONT logfont_;
  int style_;

  // Options, see the getters/setters above.
  bool directional_override_;
  bool inhibit_ligate_;
  int letter_spacing_;
  int space_width_;
  int word_spacing_;
  int justification_width_;

  // Uniscribe breaks the text into Runs. These are one length of text that is
  // in one script and one direction. This array is in reading order.
  StackVector<SCRIPT_ITEM, UNISCRIBE_STATE_STACK_RUNS> runs_;

  StackVector<Shaping, UNISCRIBE_STATE_STACK_RUNS> shapes_;

  // This is a mapping between reading order and screen order for the items.
  // Uniscribe's items array are in reading order. For right-to-left text,
  // or mixed (although WebKit's |TextRun| should really be only one
  // direction), this makes it very difficult to compute character offsets
  // and positions. This list is in screen order from left to right, and
  // gives the index into the |runs_| and |shapes_| arrays of each
  // subsequent item.
  StackVector<int, UNISCRIBE_STATE_STACK_RUNS> screen_order_;

  DISALLOW_EVIL_CONSTRUCTORS(UniscribeState);
};

}  // namespace gfx

#endif  // BASE_GFX_UNISCRIBE_H__

