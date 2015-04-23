// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/normalizedunicodetext.h"

#include <tchar.h>
#include <windows.h>
#include <winnls.h>

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_scopedptr.h"


namespace {

// Function prototypes copied from MSDN.
typedef BOOL (WINAPI *IsNormalizedStringFunction)(NORM_FORM NormForm,
                                                  LPCWSTR lpSrcString,
                                                  int cwSrcLength);
typedef int (WINAPI *NormalizeStringFunction)(NORM_FORM NormForm,
                                              LPCWSTR lpSrcString,
                                              int cwSrcLength,
                                              LPWSTR lpDstString,
                                              int cwDstLength);

// A class to provide an access to Normaliz.dll functions.
// New normalization API implemented in Normaliz.dll is available starting
// from Windows XP SP2, that's why we have to bind to it dynamically.
class NormalizationAPI {
 public:
  // Creates fully initialized NormalizationAPI object.
  // Loads DLL and binds all referenced functions.
  NormalizationAPI()
      : library_(_T("Normaliz.dll")) {
    if (library_.IsValid()) {
      is_normalized_string_.Bind(library_.handle(), "IsNormalizedString");
      normalize_string_.Bind(library_.handle(), "NormalizeString");
    }
  }

  // Proxy functions for the ones loaded from DLL.
  BOOL IsNormalizedString(NORM_FORM NormForm, LPCWSTR lpSrcString,
                          int cwSrcLength) {
    if (!is_normalized_string_.IsValid())
      return FALSE;
    return is_normalized_string_.function()(NormForm, lpSrcString, cwSrcLength);
  }
  int NormalizeString(NORM_FORM NormForm, LPCWSTR lpSrcString, int cwSrcLength,
                      LPWSTR lpDstString, int cwDstLength) {
    if (!normalize_string_.IsValid()) {
      ::SetLastError(ERROR_INVALID_FUNCTION);
      return 0;
    }
    return normalize_string_.function()(NormForm, lpSrcString, cwSrcLength,
                                        lpDstString, cwDstLength);
  }

  // Returns true if all functions were bound successfully.
  // This implies that library_ itself was loaded successfully.
  bool IsValid() const {
    return is_normalized_string_.IsValid() && normalize_string_.IsValid();
  }

 private:
  // Holds a handle to loaded Normaliz.dll.
  ScopedLibrary library_;
  // Pointers to the functions loaded from Normaliz.dll.
  FunctionFromDll<IsNormalizedStringFunction> is_normalized_string_;
  FunctionFromDll<NormalizeStringFunction> normalize_string_;

  DISALLOW_COPY_AND_ASSIGN(NormalizationAPI);
};

static NormalizationAPI normalization_api;

}  // namespace


// NormalizedUnicodeText

NormalizedUnicodeText::NormalizedUnicodeText()
    : normalized_text_(NULL) {
}


DWORD NormalizedUnicodeText::Normalize(NORM_FORM normalization_form,
                                       const WCHAR* text) {
  DWORD result = 0;
  normalized_text_ = TryToNormalizeText(normalization_form, text, &result);
  return result;
}


const WCHAR* NormalizedUnicodeText::TryToNormalizeText(
    NORM_FORM normalization_form, const WCHAR* text, DWORD *error_code) {
  if (!text) {
    text_.reset();
    return text;
  }
  _ASSERT(NULL != error_code);
  if (!error_code)
    return text;

  if (!normalization_api.IsValid()) {
    // Fall back to the previous version of normalization API.
    int folded_text_size = ::FoldStringW(MAP_PRECOMPOSED, text, -1, NULL, 0);
    if (!folded_text_size) {
      *error_code = ::GetLastError();
      return text;
    }

    text_.reset(new WCHAR[folded_text_size]);
    if (!text_.get()) {
      *error_code = ERROR_OUTOFMEMORY;
      return text;
    }

    int folding_result =
        ::FoldStringW(MAP_PRECOMPOSED, text, -1, text_.get(), folded_text_size);
    if (!folding_result) {
      *error_code = ::GetLastError();
      text_.reset();
      return text;
    }

    return text_.get();
  }

  // No need to allocate anything when text is already normalized.
  if (normalization_api.IsNormalizedString(normalization_form, text, -1))
    return text;

  // Get the first approximation for the buffer size required to store
  // normalized text.
  int normalized_text_size_guess =
      normalization_api.NormalizeString(normalization_form, text, -1, NULL, 0);

  while (normalized_text_size_guess > 0) {
    text_.reset(new WCHAR[normalized_text_size_guess]);
    if (!text_.get()) {
      *error_code = ERROR_OUTOFMEMORY;
      break;
    }

    int normalized_text_size =
        normalization_api.NormalizeString(normalization_form, text, -1,
                                          text_.get(),
                                          normalized_text_size_guess);

    if (normalized_text_size > 0) {
      // Text was successfully converted.
      return text_.get();
    }

    if (ERROR_INSUFFICIENT_BUFFER != ::GetLastError()) {
      *error_code = ::GetLastError();
      // Text cannot be normalized, use the original.
      // By the way, ERROR_SUCCESS is a puzzling case.
      // MSDN says 'The action completed successfully but yielded no results'.
      // Does this mean that output buffer was not changed?
      // Anyway, just in case, also return the original text.
      break;
    }

    // Try again with the corrected buffer size.
    normalized_text_size_guess = -normalized_text_size;
  }

  // Use the original text in case of any problem with normalization.
  text_.reset();
  return text;
}
