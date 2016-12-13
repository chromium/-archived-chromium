// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_UNICODETEXT_H_
#define BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_UNICODETEXT_H_

#include <windows.h>

#include "third_party/cld/bar/toolbar/cld/i18n/languages/public/languages.h"


// Detects a language of the UTF-16 encoded zero-terminated text.
// [in] text - UTF-16 encoded text to detect a language of.
// [in] is_plain_text - true if plain text, false otherwise (e.g. HTML).
// [out] is_reliable - true, if returned language was detected reliably.
//     See compact_lang_det.h for details.
// [out] num_languages - set to the number of languages detected on the page.
//     Language counts only if it's detected in more than 20% of the text.
// [out, optional] error_code - set to 0 in case of success, Windows
//     GetLastError() code otherwise.  Pass NULL, if not interested in errors.
// See bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det.h,
//     CompactLangDet::DetectLanguage() description for other input parameters
//     description.
// Returns: Language enum.
//     Returns NUM_LANGUAGES in case of any error.
//     See googleclient/bar/toolbar/cld/i18n/languages/internal/languages.cc
//     for details.
Language DetectLanguageOfUnicodeText(const WCHAR* text, bool is_plain_text,
                                     bool* is_reliable, int* num_languages,
                                     DWORD* error_code);


#endif  // BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_UNICODETEXT_H_
