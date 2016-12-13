// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det_impl.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_basictypes.h"

// String is "code_version - data_scrape_date"
static const char* kDetectLanguageVersion = "V1.6 - 20081121";

// Large-table version for all ~160 languages (all Tiers)

// Scan interchange-valid UTF-8 bytes and detect most likely language
Language CompactLangDet::DetectLanguage(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          bool* is_reliable) {
  bool allow_extended_lang = false;
  Language language3[3];
  int percent3[3];
  double normalized_score3[3];
  int text_bytes;
  int flags = 0;
  Language plus_one = UNKNOWN_LANGUAGE;
  const char* tld_hint = "";
  int encoding_hint = UNKNOWN_ENCODING;
  Language language_hint = UNKNOWN_LANGUAGE;

  Language lang = CompactLangDetImpl::DetectLanguageSummaryV25(
                          buffer,
                          buffer_length,
                          is_plain_text,
                          tld_hint,               // "id" boosts Indonesian
                          encoding_hint,          // SJS boosts Japanese
                          language_hint,          // ITALIAN boosts it
                          allow_extended_lang,
                          flags,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          &text_bytes,
                          is_reliable);
  // Default to English.
  if (lang == UNKNOWN_LANGUAGE) {
    lang = ENGLISH;
  }
  return lang;
}

// Scan interchange-valid UTF-8 bytes and detect list of top 3 languages.
Language CompactLangDet::DetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          Language* language3,
                          int* percent3,
                          int* text_bytes,
                          bool* is_reliable) {
  double normalized_score3[3];
  bool allow_extended_lang = false;
  int flags = 0;
  Language plus_one = UNKNOWN_LANGUAGE;
  const char* tld_hint = "";
  int encoding_hint = UNKNOWN_ENCODING;
  Language language_hint = UNKNOWN_LANGUAGE;

  Language lang = CompactLangDetImpl::DetectLanguageSummaryV25(
                          buffer,
                          buffer_length,
                          is_plain_text,
                          tld_hint,               // "id" boosts Indonesian
                          encoding_hint,          // SJS boosts Japanese
                          language_hint,          // ITALIAN boosts it
                          allow_extended_lang,
                          flags,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          text_bytes,
                          is_reliable);
  // Default to English
  if (lang == UNKNOWN_LANGUAGE) {
    lang = ENGLISH;
  }
  return lang;
}

// Same as above, with hints supplied
// Scan interchange-valid UTF-8 bytes and detect list of top 3 languages.
Language CompactLangDet::DetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          const char* tld_hint,       // "id" boosts Indonesian
                          int encoding_hint,          // SJS boosts Japanese
                          Language language_hint,     // ITALIAN boosts it
                          Language* language3,
                          int* percent3,
                          int* text_bytes,
                          bool* is_reliable) {
  double normalized_score3[3];
  bool allow_extended_lang = false;
  int flags = 0;
  Language plus_one = UNKNOWN_LANGUAGE;

  Language lang = CompactLangDetImpl::DetectLanguageSummaryV25(
                          buffer,
                          buffer_length,
                          is_plain_text,
                          tld_hint,               // "id" boosts Indonesian
                          encoding_hint,          // SJS boosts Japanese
                          language_hint,          // ITALIAN boosts it
                          allow_extended_lang,
                          flags,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          text_bytes,
                          is_reliable);
  // Default to English
  if (lang == UNKNOWN_LANGUAGE) {
    lang = ENGLISH;
  }
  return lang;
}


// Scan interchange-valid UTF-8 bytes and detect list of top 3 extended
// languages.
// Extended languages are additional Google interface languages and Unicode
// single-language scripts, from ext_lang_enc.h
Language CompactLangDet::ExtDetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          Language* language3,
                          int* percent3,
                          int* text_bytes,
                          bool* is_reliable) {
  double normalized_score3[3];
  bool allow_extended_lang = true;
  int flags = 0;
  Language plus_one = UNKNOWN_LANGUAGE;
  const char* tld_hint = "";
  int encoding_hint = UNKNOWN_ENCODING;
  Language language_hint = UNKNOWN_LANGUAGE;

  Language lang = CompactLangDetImpl::DetectLanguageSummaryV25(
                          buffer,
                          buffer_length,
                          is_plain_text,
                          tld_hint,               // "id" boosts Indonesian
                          encoding_hint,          // SJS boosts Japanese
                          language_hint,          // ITALIAN boosts it
                          allow_extended_lang,
                          flags,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          text_bytes,
                          is_reliable);
  // Do not default to English
  return lang;
}

// Same as above, with hints supplied
// Scan interchange-valid UTF-8 bytes and detect list of top 3 extended
// languages.
// Extended languages are additional Google interface languages and Unicode
// single-language scripts, from ext_lang_enc.h
Language CompactLangDet::ExtDetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          const char* tld_hint,       // "id" boosts Indonesian
                          int encoding_hint,          // SJS boosts Japanese
                          Language language_hint,     // ITALIAN boosts it
                          Language* language3,
                          int* percent3,
                          int* text_bytes,
                          bool* is_reliable) {
  double normalized_score3[3];
  bool allow_extended_lang = true;
  int flags = 0;
  Language plus_one = UNKNOWN_LANGUAGE;

  Language lang = CompactLangDetImpl::DetectLanguageSummaryV25(
                          buffer,
                          buffer_length,
                          is_plain_text,
                          tld_hint,               // "id" boosts Indonesian
                          encoding_hint,          // SJS boosts Japanese
                          language_hint,          // ITALIAN boosts it
                          allow_extended_lang,
                          flags,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          text_bytes,
                          is_reliable);
  // Do not default to English
  return lang;
}

// Same as above, and also returns internal language scores as a ratio to
// normal score for real text in that language. Scores close to 1.0 indicate
// normal text, while scores far away from 1.0 indicate badly-skewed text or
// gibberish
//
Language CompactLangDet::ExtDetectLanguageSummary(
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
                        bool* is_reliable) {
  bool allow_extended_lang = true;
  int flags = 0;
  Language plus_one = UNKNOWN_LANGUAGE;

  Language lang = CompactLangDetImpl::DetectLanguageSummaryV25(
                          buffer,
                          buffer_length,
                          is_plain_text,
                          tld_hint,               // "id" boosts Indonesian
                          encoding_hint,          // SJS boosts Japanese
                          language_hint,          // ITALIAN boosts it
                          allow_extended_lang,
                          flags,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          text_bytes,
                          is_reliable);
  // Do not default to English
  return lang;
  }



// Return version text string
// String is "code_version - data_scrape_date"
const char* CompactLangDet::DetectLanguageVersion() {
  return kDetectLanguageVersion;
}
