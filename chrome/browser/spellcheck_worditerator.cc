// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellcheck_worditerator.h"

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/string_util.h"
#include "chrome/browser/spellchecker.h"

#include "third_party/icu38/public/common/unicode/normlzr.h"
#include "third_party/icu38/public/common/unicode/schriter.h"
#include "third_party/icu38/public/common/unicode/uchar.h"
#include "third_party/icu38/public/common/unicode/uscript.h"
#include "third_party/icu38/public/common/unicode/uset.h"
#include "third_party/icu38/public/i18n/unicode/ulocdata.h"

SpellcheckCharAttribute::SpellcheckCharAttribute() {
  InitializeScriptTable();

  // Even though many dictionaries treats numbers and contractions as words and
  // treats USCRIPT_COMMON characters as word characters, the
  // SpellcheckWordIterator class treats USCRIPT_COMMON characters as non-word
  // characters to strictly-distinguish contraction characters from word
  // characters.
  SetWordScript(USCRIPT_COMMON, false);

  // Initialize the table of characters used for contractions.
  // This array consists of the 'Midletter' and 'MidNumLet' characters of the
  // word-break property list provided by Unicode, Inc.:
  //   http://www.unicode.org/Public/UNIDATA/auxiliary/WordBreakProperty.txt
  static const UChar32 kMidLetters[] = {
      L'\x003A',  // MidLetter # COLON
      L'\x00B7',  // MidLetter # MIDDLE DOT
      L'\x0387',  // MidLetter # GREEK ANO TELEIA
      L'\x05F4',  // MidLetter # HEBREW PUNCTUATION GERSHAYIM
      L'\x2027',  // MidLetter # HYPHENATION POINT
      L'\xFE13',  // MidLetter # PRESENTATION FORM FOR VERTICAL COLON
      L'\xFE55',  // MidLetter # SMALL COLON
      L'\xFF1A',  // MidLetter # FULLWIDTH COLON
      L'\x0027',  // MidNumLet # APOSTROPHE
      L'\x002E',  // MidNumLet # FULL STOP
      L'\x2018',  // MidNumLet # LEFT SINGLE QUOTATION MARK
      L'\x2019',  // MidNumLet # RIGHT SINGLE QUOTATION MARK
      L'\x2024',  // MidNumLet # ONE DOT LEADER
      L'\xFE52',  // MidNumLet # SMALL FULL STOP
      L'\xFF07',  // MidNumLet # FULLWIDTH APOSTROPHE
      L'\xFF0E',  // MidNumLet # FULLWIDTH FULL STOP
  };
  for (size_t i = 0; i < arraysize(kMidLetters); ++i)
    middle_letters_[kMidLetters[i]] = true;
}

SpellcheckCharAttribute::~SpellcheckCharAttribute() {
}

// Sets the default language for this object.
// This function retrieves the exemplar set to set up the default character
// attributes.
void SpellcheckCharAttribute::SetDefaultLanguage(
    const SpellChecker::Language& language) {
  UErrorCode status = U_ZERO_ERROR;
  ULocaleData* locale_data = ulocdata_open(language.c_str(), &status);
  if (U_FAILURE(status))
    return;

  // Retrieves the exemplar set of the given language and update the
  // character-attribute table to treat its characters as word characters.
  USet* exemplar_set = uset_open(1, 0);
  ulocdata_getExemplarSet(locale_data, exemplar_set, 0, ULOCDATA_ES_STANDARD,
                          &status);
  ulocdata_close(locale_data);
  if (U_SUCCESS(status)) {
    int length = uset_size(exemplar_set);
    for (int i = 0; i < length; ++i) {
      UChar32 character = uset_charAt(exemplar_set, i);
      SetWordScript(GetScriptCode(character), true);
    }

    // Many languages use combining characters to input their characters from
    // keyboards. On the other hand, this exemplar set does not always include
    // combining characters for such languages.
    // To treat such combining characters as word characters, we decompose
    // this exemplar set and treat the decomposed characters as word characters.
    UnicodeString composed;
    for (int i = 0; i < length; ++i)
      composed.append(uset_charAt(exemplar_set, i));

    UnicodeString decomposed;
    Normalizer::decompose(composed, FALSE, 0, decomposed, status);
    if (U_SUCCESS(status)) {
      StringCharacterIterator iterator(decomposed);
      UChar32 character = iterator.first32();
      while (character != CharacterIterator::DONE) {
        SetWordScript(GetScriptCode(character), true);
        character = iterator.next32();
      }
    }
  }
  uset_close(exemplar_set);
}

// Returns whether or not the given character is a character used by the
// selected dictionary.
bool SpellcheckCharAttribute::IsWordChar(UChar32 character) const {
  return IsWordScript(GetScriptCode(character)) && !u_isdigit(character);
}

// Returns whether or not the given character is a character used by
// contractions.
bool SpellcheckCharAttribute::IsContractionChar(UChar32 character) const {
  std::map<UChar32, bool>::const_iterator iterator;
  iterator = middle_letters_.find(character);
  if (iterator == middle_letters_.end())
    return false;
  return iterator->second;
}

// Initializes the mapping table.
void SpellcheckCharAttribute::InitializeScriptTable() {
  for (size_t i = 0; i < arraysize(script_attributes_); ++i)
    script_attributes_[i] = false;
}

// Retrieves the ICU script code.
UScriptCode SpellcheckCharAttribute::GetScriptCode(UChar32 character) const {
  UErrorCode status = U_ZERO_ERROR;
  UScriptCode script_code = uscript_getScript(character, &status);
  return U_SUCCESS(status) ? script_code : USCRIPT_INVALID_CODE;
}

// Updates the mapping table from an ICU script code to its attribute, i.e.
// whether not a script is used by the selected dictionary.
void SpellcheckCharAttribute::SetWordScript(const int script_code,
                                            bool in_use) {
  if (script_code < 0 ||
      static_cast<size_t>(script_code) >= arraysize(script_attributes_))
    return;
  script_attributes_[script_code] = in_use;
}

// Returns whether or not the given script is used by the selected
// dictionary.
bool SpellcheckCharAttribute::IsWordScript(
    const UScriptCode script_code) const {
  if (script_code < 0 ||
      static_cast<size_t>(script_code) >= arraysize(script_attributes_))
    return false;
  return script_attributes_[script_code];
}

SpellcheckWordIterator::SpellcheckWordIterator()
    : word_(NULL),
      length_(0),
      position_(0),
      allow_contraction_(false),
      attribute_(NULL) {
}

SpellcheckWordIterator::~SpellcheckWordIterator() {
}

// Initialize a word-iterator object.
void SpellcheckWordIterator::Initialize(
    const SpellcheckCharAttribute* attribute,
    const char16* word,
    size_t length,
    bool allow_contraction) {
  word_ = word;
  position_ = 0;
  length_ = static_cast<int>(length);
  allow_contraction_ = allow_contraction;
  attribute_ = attribute;
}

// Retrieves a word (or a contraction).
// When a contraction is enclosed with contraction characters (e.g. 'isn't',
// 'rock'n'roll'), we should discard the beginning and the end of the
// contraction but we should never split the contraction.
// To handle this case easily, we should firstly extract a segment consisting
// of word characters and contraction characters, and discard contraction
// characters at the beginning and the end of the extracted segment.
bool SpellcheckWordIterator::GetNextWord(string16* word_string,
                                         int* word_start,
                                         int* word_length) {
  word_string->empty();
  *word_start = 0;
  *word_length = 0;
  while (position_ < length_) {
    int segment_start = 0;
    int segment_end = 0;
    GetSegment(&segment_start, &segment_end);
    TrimSegment(segment_start, segment_end, word_start, word_length);
    if (*word_length > 0)
      return Normalize(*word_start, *word_length, word_string);
  }

  return false;
}

// Retrieves a segment consisting of word characters (and contraction
// characters if the |allow_contraction_| value is true).
// When the current position refers to a non-word character, this function
// returns a non-empty segment consisting of the character itself. In this
// case, the TrimSegment() function discards the character and returns an
// empty word (i.e. |word_length| == 0).
void SpellcheckWordIterator::GetSegment(int* segment_start,
                                        int* segment_end) {
  int position = position_;
  while (position <  length_) {
    UChar32 character;
    U16_NEXT(word_, position, length_, character);
    if (!attribute_->IsWordChar(character)) {
      if (!allow_contraction_ || !attribute_->IsContractionChar(character))
        break;
    }
  }
  *segment_start = position_;
  *segment_end = position;
  position_ = position;
}

// Discards non-word characters at the beginning and the end of the given
// segment.
void SpellcheckWordIterator::TrimSegment(int segment_start,
                                         int segment_end,
                                         int* word_start,
                                         int* word_length) const {
  while (segment_start < segment_end) {
    UChar32 character;
    int segment_next = segment_start;
    U16_NEXT(word_, segment_next, segment_end, character);
    if (attribute_->IsWordChar(character)) {
      *word_start = segment_start;
      break;
    }
    segment_start = segment_next;
  }
  while (segment_end >= segment_start) {
    UChar32 character;
    int segment_prev = segment_end;
    U16_PREV(word_, segment_start, segment_prev, character);
    if (attribute_->IsWordChar(character)) {
      *word_length = segment_end - segment_start;
      break;
    }
    segment_end = segment_prev;
  }
}

// Normalizes a non-terminated string into its canonical form so that
// a spellchecker object can check spellings of words which contain ligatures,
// full-width letters, etc.
// USCRIPT_LATIN does not only consists of US-ASCII and ISO/IEC 8859-1, but
// also consists of ISO/IEC 8859-{2,3,4,9,10}, ligatures, fullwidth latin,
// etc. For its details, please read the script table in
// "http://www.unicode.org/Public/UNIDATA/Scripts.txt".
bool SpellcheckWordIterator::Normalize(int input_start,
                                       int input_length,
                                       string16* output_string) const {
  // Unicode Standard Annex #15 "http://www.unicode.org/unicode/reports/tr15/"
  // does not only write NFKD and NFKC can compose ligatures into their ASCII
  // alternatives, but also write NFKC keeps accents of characters.
  // Therefore, NFKC seems to be the best option for hunspell.
  UnicodeString input(FALSE, &word_[input_start], input_length);
  UErrorCode status = U_ZERO_ERROR;
  UnicodeString output;
  Normalizer::normalize(input, UNORM_NFKC, 0, output, status);
  if (U_SUCCESS(status))
    output_string->assign(output.getTerminatedBuffer());
  return (status == U_ZERO_ERROR);
}
