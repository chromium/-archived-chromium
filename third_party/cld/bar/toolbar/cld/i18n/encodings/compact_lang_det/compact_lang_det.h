// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE:
// This code has not yet been evaluated against LangId, which is the official
// production language identification system. However, it seems to be of
// similar precison overall, and it covers all the Google languages in
//   i18n/languages/proto/languages.proto
// except the four Creoles_and_Pigins.

// Baybayin (ancient script of the Philippines) is detected as TAGALOG.
// Chu Nom (Vietnamese ancient Han characters) is detected as VIETNAMESE.
// HAITIAN_CREOLE is detected as such.
// NORWEGIAN and NORWEGIAN_N are detected separately (but not robustly)
// PORTUGUESE, PORTUGUESE_P, and PORTUGUESE_B are all detected as PORTUGUESE.
// ROMANIAN-Latin is detected as ROMANIAN; ROMANIAN-Cyrillic as MOLDAVIAN.
// SERBO_CROATIAN, BOSNIAN, CROATIAN, SERBIAN, MONTENEGRIN in the Latin script
//  are all detected as CROATIAN; in the Cyrillic script as SERBIAN.
// Zhuang is detected in the Latin script only.
//
// The Google interface languages X_PIG_LATIN and X_KLINGON are detected in the
//  extended calls ExtDetectLanguageSummary(). BorkBorkBork, ElmerFudd, and
//  Hacker are not detected (too little training data).
//
// UNKNOWN_LANGUAGE is returned if no language's internal reliablity measure
//  is high enough. This happens with non-text input such as the bytes of a
//  JPEG, and also with some text in languages outside the Google Language
//  enum, such as Ilonggo.
//
// The following languages are detected in multiple scripts:
//  AZERBAIJANI (Latin, Cyrillic*, Arabic*)
//  BURMESE (Latin, Myanmar)
//  HAUSA (Latin, Arabic)
//  KASHMIRI (Arabic, Devanagari)
//  KAZAKH (Latin, Cyrillic, Arabic)
//  KURDISH (Latin*, Arabic)
//  KYRGYZ (Cyrillic, Arabic)
//  LIMBU (Devanagari, Limbu)
//  MONGOLIAN (Cyrillic, Mongolian)
//  SANSKRIT (Latin, Devanagari)
//  SINDHI (Arabic, Devanagari)
//  TAGALOG (Latin, Tagalog)
//  TAJIK (Cyrillic, Arabic*)
//  TATAR (Latin, Cyrillic, Arabic)
//  TURKMEN (Latin, Cyrillic, Arabic)
//  UIGHUR (Latin, Cyrillic, Arabic)
//  UZBEK (Latin, Cyrillic, Arabic)
//
// * Due to a shortage of training text, AZERBAIJANI is not currently detected
//   in Arabic or Cyrillic scripts, nor KURDISH in Latin script, nor TAJIK in
//   Arabic script.
//

#ifndef I18N_ENCODINGS_COMPACT_LANG_DET_COMPACT_LANG_DET_H_
#define I18N_ENCODINGS_COMPACT_LANG_DET_COMPACT_LANG_DET_H_

#include "third_party/cld/bar/toolbar/cld/i18n/languages/public/languages.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det_impl.h"

namespace CompactLangDet {
  // Scan interchange-valid UTF-8 bytes and detect most likely language,
  // or set of languages.
  //
  // Design goals:
  //   Skip over big stretches of HTML tags
  //   Able to return ranges of different languages
  //   Relatively small tables and relatively fast processing
  //   Thread safe
  //
  // For HTML documents, tags are skipped, along with <script> ... </script>
  // and <style> ... </style> sequences, and entities are expanded.
  //
  // We distinguish between bytes of the raw input buffer and bytes of non-tag
  // text letters. Since tags can be over 50% of the bytes of an HTML Page,
  // and are nearly all seven-bit ASCII English, we prefer to distinguish
  // language mixture fractions based on just the non-tag text.
  //
  // Inputs: text and text_length
  //  Code skips HTML tags and expands HTML entities, unless
  //  is_plain_text is true
  // Outputs:
  //  language3 is an array of the top 3 languages or UNKNOWN_LANGUAGE
  //  percent3 is an array of the text percentages 0..100 of the top 3 languages
  //  text_bytes is the amount of non-tag/letters-only text found
  //  is_reliable set true if the returned Language is some amount more
  //  probable then the second-best Language. Calculation is a complex function
  //  of the length of the text and the different-script runs of text.
  // Return value: the most likely Language for the majority of the input text
  //  Length 0 input returns UNKNOWN_LANGUAGE. Very short indeterminate text
  //  defaults to ENGLISH.
  //
  // The first two versions return ENGLISH instead of UNKNOWN_LANGUAGE, for
  // backwards compatibility with LLD.
  //
  // The third version may return UNKNOWN_LANGUAGE, and also returns extended
  // language codes from ext_lang_enc.h
  //
  // Subsetting: For fast detection over large documents, these routines will
  // scan non-tag text of the initial part of a document, then will
  // skip 4-16 bytes and subsample text in the rest of the document, up to a
  // fixed limit (currently 160KB of non-tag letters).
  //

  // Scan interchange-valid UTF-8 bytes and detect most likely language
  Language DetectLanguage(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          bool* is_reliable);

  // Scan interchange-valid UTF-8 bytes and detect list of top 3 languages.
  // language3[0] is also the return value
  Language DetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          Language* language3,
                          int* percent3,
                          int* text_bytes,
                          bool* is_reliable);

  // Same as above, with hints supplied
  // Scan interchange-valid UTF-8 bytes and detect list of top 3 languages.
  // language3[0] is also the return value
  Language DetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          const char* tld_hint,       // "id" boosts Indonesian
                          int encoding_hint,          // SJS boosts Japanese
                          Language language_hint,     // ITALIAN boosts it
                          Language* language3,
                          int* percent3,
                          int* text_bytes,
                          bool* is_reliable);

  // Scan interchange-valid UTF-8 bytes and detect list of top 3 extended
  // languages.
  //
  // Extended languages are additional Google interface languages and Unicode
  // single-language scripts, from ext_lang_enc.h. They are experimental and
  // this call may be removed.
  //
  // language3[0] is also the return value
  Language ExtDetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          Language* language3,
                          int* percent3,
                          int* text_bytes,
                          bool* is_reliable);

  // Same as above, with hints supplied
  // Scan interchange-valid UTF-8 bytes and detect list of top 3 extended
  // languages.
  //
  // Extended languages are additional Google interface languages and Unicode
  // single-language scripts, from ext_lang_enc.h. They are experimental and
  // this call may be removed.
  //
  // language3[0] is also the return value
  Language ExtDetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          const char* tld_hint,       // "id" boosts Indonesian
                          int encoding_hint,          // SJS boosts Japanese
                          Language language_hint,     // ITALIAN boosts it
                          Language* language3,
                          int* percent3,
                          int* text_bytes,
                          bool* is_reliable);

  // Same as above, and also returns internal language scores as a ratio to
  // normal score for real text in that language. Scores close to 1.0 indicate
  // normal text, while scores far away from 1.0 indicate badly-skewed text or
  // gibberish
  //
  Language ExtDetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          const char* tld_hint,       // "id" boosts Indonesian
                          int encoding_hint,          // SJS boosts Japanese
                          Language language_hint,     // ITALIAN boosts it
                          Language* language3,
                          int* percent3,
                          double* normalized_score3,
                          int* text_bytes,
                          bool* is_reliable);

  // Return version text string
  // String is "code_version - data_scrape_date"
  const char* DetectLanguageVersion();
};      // End namespace CompactLangDet

#endif  // I18N_ENCODINGS_COMPACT_LANG_DET_COMPACT_LANG_DET_H_
