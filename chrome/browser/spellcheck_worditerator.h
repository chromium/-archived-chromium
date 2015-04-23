// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECK_WORDITERATOR_H_
#define CHROME_BROWSER_SPELLCHECK_WORDITERATOR_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/string16.h"

#include "unicode/uscript.h"

// A class which handles character attributes dependent on a spellchecker and
// its dictionary.
// This class is used by the SpellcheckWordIterator class to determine whether
// or not a character is one used by the spellchecker and its dictinary.
class SpellcheckCharAttribute {
 public:
  SpellcheckCharAttribute();

  ~SpellcheckCharAttribute();

  // Sets the default language of the spell checker. This controls which
  // characters are considered parts of words of the given language.
  void SetDefaultLanguage(const std::string& language);

  // Returns whether or not the given character is a character used by the
  // selected dictionary.
  // Parameters
  //   * character [in] (UChar32)
  //     Represents a Unicode character to be checked.
  // Return values
  //   * true
  //     The given character is a word character.
  //   * false
  //     The given character is not a word character.
  bool IsWordChar(UChar32 character) const;

  // Returns whether or not the given character is a character used by
  // contractions.
  // Parameters
  //   * character [in] (UChar32)
  //     Represents a Unicode character to be checked.
  // Return values
  //   * true
  //     The given character is a character used by contractions.
  //   * false
  //     The given character is not a character used by contractions.
  bool IsContractionChar(UChar32 character) const;

 private:
  // Initializes the mapping table.
  void InitializeScriptTable();

  // Retrieves the ICU script code.
  UScriptCode GetScriptCode(UChar32 character) const;

  // Updates an entry in the mapping table.
  void SetWordScript(const int script_code, bool in_use);

  // Returns whether or not the given script is used by the selected
  // dictionary.
  bool IsWordScript(const UScriptCode script_code) const;

 private:
  // Represents a mapping table from a script code to a boolean value
  // representing whether or not the script is used by the selected dictionary.
  bool script_attributes_[USCRIPT_CODE_LIMIT];

  // Represents a table of characters used by contractions.
  std::map<UChar32, bool> middle_letters_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckCharAttribute);
};

// A class which implements methods for finding the location of word boundaries
// used by the Spellchecker class.
// This class is implemented on the following assumptions:
//   * An input string is encoded in UTF-16 (i.e. it may contain surrogate
//     pairs), and;
//   * The length of a string is the number of UTF-16 characters in the string
//     (i.e. the length of a non-BMP character becomes two).
class SpellcheckWordIterator {
 public:
  SpellcheckWordIterator();

  ~SpellcheckWordIterator();

  // Initializes a word-iterator object.
  // Parameters
  //   * attribute [in] (const SpellcheckCharAttribute*)
  //     Represents a set of character attributes used for filtering out
  //     non-word characters.
  //   * word [in] (const char16*)
  //     Represents a string from which this object extracts words.
  //     (This string does not have to be NUL-terminated.)
  //   * length [in] (size_t)
  //     Represents the length of the given string, in UTF-16 characters.
  //     This value should not include terminating NUL characters.
  //   * allow_contraction [in] (bool)
  //     Represents a flag to control whether or not this object should split a
  //     possible contraction (e.g. "isn't", "in'n'out", etc.)
  // Return values
  //   * true
  //     This word-iterator object is initialized successfully.
  //   * false
  //     An error occured while initializing this object.
  void Initialize(const SpellcheckCharAttribute* attribute,
                  const char16* word,
                  size_t length,
                  bool allow_contraction);

  // Retrieves a word (or a contraction).
  // Parameters
  //   * word_string [out] (string16*)
  //     Represents a word (or a contraction) to be checked its spelling.
  //     This |word_string| has been already normalized to its canonical form
  //     (i.e. decomposed ligatures, replaced full-width latin characters to
  //     its ASCII alternatives, etc.) so that a SpellChecker object can check
  //     its spelling without any additional operations.
  //     On the other hand, a substring of the input string
  //       string16 str(&word[word_start], word_length);
  //     represents the non-normalized version of this extracted word.
  //   * word_start [out] (int*)
  //     Represents the offset of this word from the beginning of the input
  //     string, in UTF-16 characters.
  //   * word_length [out] (int*)
  //     Represents the length of an extracted word before normalization, in
  //     UTF-16 characters.
  //     When the input string contains ligatures, this value may not be equal
  //     to the length of the |word_string|.
  // Return values
  //   * true
  //     Found a word (or a contraction) to be checked its spelling.
  //   * false
  //     Not found any more words or contractions to be checked their spellings.
  bool GetNextWord(string16* word_string,
                   int* word_start,
                   int* word_length);

 private:
  // Retrieves a segment consisting of word characters (and contraction
  // characters if the |allow_contraction| value is true).
  void GetSegment(int* segment_start,
                  int* segment_end);

  // Discards non-word characters at the beginning and the end of the given
  // segment.
  void TrimSegment(int segment_start,
                   int segment_end,
                   int* word_start,
                   int* word_length) const;

  // Normalizes the given segment of the |word_| variable and write its
  // canonical form to the |output_string|.
  bool Normalize(int input_start,
                 int input_length,
                 string16* output_string) const;

 private:
  // The pointer to the input string from which we are extracting words.
  const char16* word_;

  // The length of the original string.
  int length_;

  // The current position in the original string.
  int position_;

  // The flag to control whether or not this object should extract possible
  // contractions.
  bool allow_contraction_;

  // The character attributes used for filtering out non-word characters.
  const SpellcheckCharAttribute* attribute_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckWordIterator);
};

#endif  // CHROME_BROWSER_SPELLCHECK_WORDITERATOR_H_
