// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_unicodetext.h"

#include <tchar.h>
#include <windows.h>

#include <vector>  // to compile bar/common/component.h

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_scopedptr.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/normalizedunicodetext.h"


// Detects a language of the UTF-16 encoded zero-terminated text.
// Returns: Language enum.
// TODO : make it reuse already allocated buffers to avoid excessive
// allocate/free call pairs.  The idea is to have two buffers allocated and
// alternate their use for every Windows API call.
// Let's leave it as it is, simple and working and optimize it as the next step
// if it will consume too much resources (after careful measuring, indeed).
Language DetectLanguageOfUnicodeText(const WCHAR* text, bool is_plain_text,
                                     bool* is_reliable, int* num_languages,
                                     DWORD* error_code) {
  if (!text || !num_languages) {
    if (error_code)
      *error_code = ERROR_INVALID_PARAMETER;
    return NUM_LANGUAGES;
  }

  // Normalize text first.  We do not check the return value here since there
  // is no meaningful recovery we can do in case of failure anyway.
  // Since the vast majority of texts on the Internet is already normalized
  // and languages which require normalization are easy to recognize by CLD
  // anyway, we'll benefit more from trying to detect language in non-normalized
  // text (and, with some probability, fail to recognize it) than to give up
  // right away and return the unknown language here.
  NormalizedUnicodeText nomalized_text;
  nomalized_text.Normalize(NormalizationC, text);

  // Determine the size of the buffer required to store a lowercased text.
  int lowercase_text_size =
      ::LCMapString(NULL, LCMAP_LOWERCASE | LCMAP_LINGUISTIC_CASING,
                    nomalized_text.get(), -1,
                    NULL, 0);
  if (!lowercase_text_size) {
    if (error_code)
      *error_code = ::GetLastError();
    return NUM_LANGUAGES;
  }

  scoped_array<WCHAR> lowercase_text(new WCHAR[lowercase_text_size]);
  if (!lowercase_text.get())
    return NUM_LANGUAGES;

  // Covert text to lowercase.
  int lowercasing_result =
      ::LCMapString(NULL, LCMAP_LOWERCASE | LCMAP_LINGUISTIC_CASING,
                    nomalized_text.get(), -1,
                    lowercase_text.get(), lowercase_text_size);
  if (!lowercasing_result) {
    if (error_code)
      *error_code = ::GetLastError();
    return NUM_LANGUAGES;
  }

  // Determine the size of the buffer required to covert text to UTF-8.
  int utf8_encoded_buffer_size =
      ::WideCharToMultiByte(CP_UTF8, 0,
                            lowercase_text.get(), -1,
                            NULL, 0,
                            NULL, NULL);
  if (!utf8_encoded_buffer_size) {
    if (error_code)
      *error_code = ::GetLastError();
    return NUM_LANGUAGES;
  }

  scoped_array<char> utf8_encoded_buffer(
      new char[utf8_encoded_buffer_size]);

  // Convert text to UTF-8.
  int utf8_encoding_result =
      ::WideCharToMultiByte(CP_UTF8, 0,
                            lowercase_text.get(), -1,
                            utf8_encoded_buffer.get(), utf8_encoded_buffer_size,
                            NULL, NULL);
  if (!utf8_encoding_result) {
    if (error_code)
      *error_code = ::GetLastError();
    return NUM_LANGUAGES;
  }

  if (error_code)
    *error_code = 0;

  // Engage core CLD library language detection.
  Language language3[3] = {
    UNKNOWN_LANGUAGE, UNKNOWN_LANGUAGE, UNKNOWN_LANGUAGE
  };
  int percent3[3] = { 0, 0, 0 };
  int text_bytes = 0;
  // We ignore return value here due to the problem described in bug 1800161.
  // For example, translate.google.com was detected as Indonesian.  It happened
  // due to the heuristic in CLD, which ignores English as a top language
  // in the presence of another reliably detected language.
  // See the actual code in compact_lang_det_impl.cc, CalcSummaryLang function.
  // language3 array is always set according to the detection results and
  // is not affected by this heuristic.
  CompactLangDet::DetectLanguageSummary(utf8_encoded_buffer.get(),
                                        utf8_encoded_buffer_size,
                                        is_plain_text, language3, percent3,
                                        &text_bytes, is_reliable);

  // Calcualte a number of languages detected in more than 20% of the text.
  const int kMinTextPercentToCountLanguage = 20;
  *num_languages = 0;
  COMPILE_ASSERT(ARRAYSIZE(language3) == ARRAYSIZE(percent3),
                 language3_and_percent3_should_be_of_the_same_size);
  for (int i = 0; i < ARRAYSIZE(language3); ++i) {
    if (IsValidLanguage(language3[i]) && !IS_LANGUAGE_UNKNOWN(language3[i]) &&
        percent3[i] >= kMinTextPercentToCountLanguage) {
      ++*num_languages;
    }
  }

  return language3[0];
}
