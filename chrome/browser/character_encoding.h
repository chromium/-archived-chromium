// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_CHARACTER_ENCODING_H__
#define CHROME_BROWSER_CHARACTER_ENCODING_H__

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"

class CharacterEncoding {
 public:
  // Enumeration of the types of Browser encoding name we
  // currently support. This is defined outside of Browser
  // to avoid cyclical dependencies.

  // Return canonical encoding name according to the command ID.
  // THIS FUNCTION IS NOT THREADSAFE. You must run this function
  // only in UI thread.
  static std::wstring GetCanonicalEncodingNameByCommandId(int id);

  // Return display name of canonical encoding according to the command
  // ID. THIS FUNCTION IS NOT THREADSAFE. You must run this function
  // only in UI thread.
  static std::wstring GetCanonicalEncodingDisplayNameByCommandId(int id);

  // Return count number of all supported canonical encoding.
  static int GetSupportCanonicalEncodingCount();

  // Return canonical encoding name according to the index, which starts
  // from zero to GetSupportCanonicalEncodingCount() - 1. THIS FUNCTION
  // IS NOT THREADSAFE. You must run this function only in UI thread.
  static std::wstring GetCanonicalEncodingNameByIndex(int index);

  // Return display name of canonical encoding according to the index,
  // which starts from zero to GetSupportCanonicalEncodingCount() - 1.
  // THIS FUNCTION IS NOT THREADSAFE. You must run this function
  // only in UI thread.
  static std::wstring GetCanonicalEncodingDisplayNameByIndex(int index);

  // Return canonical encoding name according to the encoding alias name. THIS
  // FUNCTION IS NOT THREADSAFE. You must run this function only in UI thread.
  static std::wstring GetCanonicalEncodingNameByAliasName(
      const std::wstring& alias_name);

  // Returns the pointer of a vector of command ids corresponding to
  // encodings to display in the encoding menu. The list begins with
  // the locale-dependent static encodings and recently selected
  // encodings is followed by the rest of encodings belonging to neither.
  // The vector will be created and destroyed by CharacterEncoding.
  // The returned std::vector is maintained by this class. The parameter
  // |locale_encodings| is string of static encodings list which is from the
  // corresponding string resource that is stored in the resource bundle.
  // The parameter |recently_select_encodings| is string of encoding list which
  // is from user recently selected. THIS FUNCTION IS NOT THREADSAFE. You must
  // run this function only in UI thread.
  static const std::vector<int>* GetCurrentDisplayEncodings(
      const std::wstring& locale_encodings,
      const std::wstring& recently_select_encodings);

  // This function is for updating |original_selected_encoding_list| with a
  // |new_selected_encoding_id|. If the encoding is already in the original
  // list, then returns false. Otherwise |selected_encoding_list| will return a
  // new string for user selected encoding short list and function returns true.
  static bool UpdateRecentlySelectdEncoding(
      const std::wstring& original_selected_encodings,
      int new_selected_encoding_id,
      std::wstring* selected_encodings);

 private:
  // Disallow instantiating it since this class only contains static methods.
  DISALLOW_IMPLICIT_CONSTRUCTORS(CharacterEncoding);
};

#endif  // CHROME_BROWSER_CHARACTER_ENCODING_H__
