// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef I18N_ENCODINGS_COMPACT_LANG_DET_COMPACT_LANG_DET_IMPL_H_
#define I18N_ENCODINGS_COMPACT_LANG_DET_COMPACT_LANG_DET_IMPL_H_

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/lang_enc.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_basictypes.h"


static const int kCLDFlagFinish = 1;
static const int kCLDFlagSqueeze = 2;
static const int kCLDFlagRepeats = 4;
static const int kCLDFlagTop40 = 8;
static const int kCLDFlagShort = 16;
static const int kCLDFlagHint = 32;   // Experimental, undebugged
static const int kCLDFlagUseWords = 64;

/***

Flag meanings:

Flags are used in the context of a recursive call from Detect to itself,
trying to deal in a more restrictive way with input that was not reliably
identified in the top-level call.

Finish -- Do not further recurse; return whatever result ensues, even if it is
          unreliable. Typically set in any recursive call to take a second try
          on unreliable text.

Squeeze -- For each text run, do an inplace cheapsqueeze to remove chunks of
          highly repetitive text and chunks of text with too many 1- and
          2-letter words. This avoids scoring repetitive or useless non-text
          crap in large files such bogus JPEGs within an HTML file.

Repeats -- When scoring a text run, do a cheap prediction of each character
          and do not score a unigram/quadgram if the last character of same is
          correctly predicted. This is a slower, finer-grained form of
          cheapsqueeze, typically used when the first pass got unreliable
          results.

Top40 -- Restrict the set of scored languages to the Google "Top 40*", which is
          actually 38 languages. This gets rid of about 110 language that
          represent about 0.7% of the web. Typically used when the first pass
          got unreliable results.

Short -- Use trigram (three letter) scoring instad of quadgrams. Restricted to
          the top 40* languages, Latin and Cyrillic scripts only.
          Not as precise as quadgrams, but it gives some plausible result on
          1- or 2-word text in major languages.

Hint -- EXPERIMENTAL flag for compact_lang_det_test.cc to indicate a language
          hint supplied in parameter plus_one.

UseWords -- In additon to scoring quad/uni/nil-grams, score complete words

Tentative decision logic:

In the middle of first pass -- After 4KB of text, look at the front 256 bytes
          of every full 4KB buffer. If it compresses very well (say 3:1) or has
          lots of spaces (say 1 of every 4 bytes), assume that the input is
          large and contains lots of bogus non-text. Recurse, passing the
          Squeeze flag to strip out chunks of this non-text.

At the end of the first pass --
          If the top language is reliable and >= 70% of the document, return.
          Else if the top language is reliable and top+2nd >= say 94%, return.
          Else, either the top language is not reliable or there is a lot of
          other crap.
***/



namespace CompactLangDetImpl {
  // Scan interchange-valid UTF-8 bytes and detect most likely language,
  // or set of languages.
  //
  // Design goals:
  //   Skip over big stretches of HTML tags
  //   Able to return ranges of different languages
  //   Relatively small tables and relatively fast processing
  //   Thread safe
  //

  typedef struct {
    int perscript_count;
    const Language* perscript_lang;
  } PerScriptPair;

  typedef struct {
    // Constants for hashing 4-7 byte quadgram to 32 bits
    const int kQuadHashB4Shift;
    const int kQuadHashB4bShift;
    const int kQuadHashB5Shift;
    const int kQuadHashB5bShift;
    // Constants for hashing 32 bits to kQuadKeyTable subscript/key
    const int kHashvalToSubShift;
    const uint32 kHashvalToSubMask;
    const int kHashvalToKeyShift;
    const uint32 kHashvalToKeyMask;
    const int kHashvalAssociativity;
    // Pointers to the actual tables
    const PerScriptPair* kPerScriptPair;
    const uint16* kQuadKeyTable;
    const uint32* kQuadValueTable;
  } LangDetObj;

  // For HTML documents, tags are skipped, along with <script> ... </script>
  // and <style> ... </style> sequences, and entities are expanded.
  //
  // We distinguish between bytes of the raw input buffer and bytes of non-tag
  // text letters. Since tags can be over 50% of the bytes of an HTML Page,
  // and are nearly all seven-bit ASCII English, we prefer to distinguish
  // language mixture fractions based on just the non-tag text.
  //
  // Inputs: text and text_length
  //  is_plain_text if true says to NOT parse/skip HTML tags nor entities
  // Outputs:
  //  language3 is an array of the top 3 languages or UNKNOWN_LANGUAGE
  //  percent3 is an array of the text percentages 0..100 of the top 3 languages
  //  normalized_score3 is an array of internal scores, normalized to the
  //    average score for each language over a body of training text. A
  //    normalized score significantly away from 1.0 indicates very skewed text
  //    or gibberish.
  //
  //  text_bytes is the amount of non-tag/letters-only text found
  //  is_reliable set true if the returned Language is at least 2**30 times more
  //  probable then the second-best Language
  //
  // Return value: the most likely Language for the majority of the input text
  //  Length 0 input and text with no reliable letter sequences returns
  //  UNKNOWN_LANGUAGE
  //
  // Subsetting: For fast detection over large documents, these routines will
  // scan non-tag text of the initial part of a document, then will
  // skip 4-16 bytes and subsample text in the rest of the document, up to a
  // fixed limit (currently 160KB of non-tag letters).
  //

  Language DetectLanguageSummaryV25(
                        const char* buffer,
                        int buffer_length,
                        bool is_plain_text,
                        const char* tld_hint,       // "id" boosts Indonesian
                        int encoding_hint,          // SJS boosts Japanese
                        Language language_hint,     // ITALIAN boosts it
                        bool allow_extended_lang,
                        int flags,
                        Language plus_one,
                        Language* language3,
                        int* percent3,
                        double* normalized_score3,
                        int* text_bytes,
                        bool* is_reliable);

  // For unit testing:
  // Remove portions of text that have a high density of spaces, or that are
  // overly repetitive, squeezing the remaining text in-place to the front
  // of the input buffer.
  // Return the new, possibly-shorter length
  int CheapSqueezeInplace(char* isrc, int srclen, int ichunksize);
};      // End namespace CompactLangDetImpl

#endif  // I18N_ENCODINGS_COMPACT_LANG_DET_COMPACT_LANG_DET_IMPL_H_
