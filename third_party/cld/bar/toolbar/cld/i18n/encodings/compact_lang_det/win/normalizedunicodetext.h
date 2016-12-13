// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_NORMALIZEDUNICODETEXT_H_
#define BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_NORMALIZEDUNICODETEXT_H_

#include <tchar.h>
#include <windows.h>

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_scopedptr.h"


#if (WINVER < 0x0600)
// Copied from winnls.h, we're not using the latest SDK yet.
typedef enum _NORM_FORM {
  NormalizationOther  = 0,
  NormalizationC = 0x1,
  NormalizationD = 0x2,
  NormalizationKC = 0x5,
  NormalizationKD = 0x6
} NORM_FORM;
#endif


// Gives you back a normalized version of the input text.  Normalization is
// performed to the specified form.
// Instance lifetime should be within the lifetime span of the 'text'.
class NormalizedUnicodeText {
 public:
  // Creates an empty instance of NormalizedUnicodeText.
  NormalizedUnicodeText();

  // Creates a fully initialized instance of NormalizedUnicodeText.
  // [in] normalization_form - normalization rule set (see MSDN for details).
  // [in] text - zero-terminated UTF-16 encoded string.
  // Returns 0 in case of success, Win32 error code in case of failure.
  //     In case of failure, get() returns the original text.
  DWORD Normalize(NORM_FORM normalization_form, const WCHAR* text);

  // Returns pointer to the normalized text.
  const WCHAR* get() const { return normalized_text_; }

 private:
  // Normalizes 'text' by the 'normalization_form' rules.
  // [in] normalization_form - normalization rule set (see MSDN for details).
  // [in] text - zero-terminated UTF-16 encoded string.
  // [out] error_code - Win32 error code.
  const WCHAR* TryToNormalizeText(NORM_FORM normalization_form,
                                  const WCHAR* text, DWORD *error_code);

  // Pointer to the normalized text.
  const WCHAR* normalized_text_;
  // When the source text is already normalized by the requested normalization
  // form, text_ is not used and normalized_text_ just points to the source
  // text. When the source text requres normalization, text_ contains normalized
  // version of the source text and normalized_text_ points to this buffer.
  // Since CLD requires NormalizationC form and the overwhelming majority of all
  // texts in the Internet is already normalized to this form, it's expected
  // that this class will not introduce any runtime memory overhead.
  scoped_array<WCHAR> text_;

  DISALLOW_COPY_AND_ASSIGN(NormalizedUnicodeText);
};


#endif  // BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_NORMALIZEDUNICODETEXT_H_
