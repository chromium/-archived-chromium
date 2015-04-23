// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File utilities that use the ICU library go in this file.  Functions using ICU
// are separated from the other functions to prevent ICU being pulled in by the
// linker if there is a false dependency.
//
// (The VS2005 linker finds such a false dependency and adds ~300K of ICU to
// chrome.exe if this code lives in file_util.cc, even though none of this code
// is called.)

#include "base/file_util.h"

#include "base/singleton.h"
#include "base/string_util.h"
#include "unicode/uniset.h"

namespace {
class IllegalCharacters {
 public:
  bool contains(UChar32 ucs4) {
    return !!set->contains(ucs4);
  }

  bool containsNone(const string16 &s) {
    return !!set->containsNone(UnicodeString(s.c_str(), s.size()));
  }

 private:
  friend class Singleton<IllegalCharacters>;
  friend struct DefaultSingletonTraits<IllegalCharacters>;

  IllegalCharacters();
  ~IllegalCharacters() { }

  scoped_ptr<UnicodeSet> set;

  DISALLOW_COPY_AND_ASSIGN(IllegalCharacters);
};

IllegalCharacters::IllegalCharacters() {
  UErrorCode status = U_ZERO_ERROR;
  // Control characters, formatting characters, non-characters, and
  // some printable ASCII characters regarded as dangerous ('"*/:<>?\\').
  // See  http://blogs.msdn.com/michkap/archive/2006/11/03/941420.aspx
  // and http://msdn2.microsoft.com/en-us/library/Aa365247.aspx
  // TODO(jungshik): Revisit the set. ZWJ and ZWNJ are excluded because they
  // are legitimate in Arabic and some S/SE Asian scripts. However, when used
  // elsewhere, they can be confusing/problematic.
  // Also, consider wrapping the set with our Singleton class to create and
  // freeze it only once. Note that there's a trade-off between memory and
  // speed.
#if defined(WCHAR_T_IS_UTF16)
  set.reset(new UnicodeSet(UnicodeString(
      L"[[\"*/:<>?\\\\|][:Cc:][:Cf:] - [\u200c\u200d]]"), status));
#else
  set.reset(new UnicodeSet(UNICODE_STRING_SIMPLE(
      "[[\"*/:<>?\\\\|][:Cc:][:Cf:] - [\\u200c\\u200d]]").unescape(),
      status));
#endif
  DCHECK(U_SUCCESS(status));
  // Add non-characters. If this becomes a performance bottleneck by
  // any chance, do not add these to |set| and change IsFilenameLegal()
  // to check |ucs4 & 0xFFFEu == 0xFFFEu|, in addiition to calling
  // containsNone().
  set->add(0xFDD0, 0xFDEF);
  for (int i = 0; i <= 0x10; ++i) {
    int plane_base = 0x10000 * i;
    set->add(plane_base + 0xFFFE, plane_base + 0xFFFF);
  }
  set->freeze();
}

}  // namespace

namespace file_util {

bool IsFilenameLegal(const string16& file_name) {
  return Singleton<IllegalCharacters>()->containsNone(file_name);
}

void ReplaceIllegalCharacters(std::wstring* file_name, int replace_char) {
  DCHECK(file_name);

  DCHECK(!(Singleton<IllegalCharacters>()->contains(replace_char)) &&
         replace_char < 0x10000);

  // Remove leading and trailing whitespace.
  TrimWhitespace(*file_name, TRIM_ALL, file_name);

  if (IsFilenameLegal(WideToUTF16(*file_name)))
    return;

  std::wstring::size_type i = 0;
  std::wstring::size_type length = file_name->size();
  const wchar_t* wstr = file_name->data();
#if defined(WCHAR_T_IS_UTF16)
  // Using |span| method of UnicodeSet might speed things up a bit, but
  // it's not likely to matter here.
  std::wstring temp;
  temp.reserve(length);
  while (i < length) {
    UChar32 ucs4;
    std::wstring::size_type prev = i;
    U16_NEXT(wstr, i, length, ucs4);
    if (Singleton<IllegalCharacters>()->contains(ucs4)) {
      temp.push_back(replace_char);
    } else if (ucs4 < 0x10000) {
      temp.push_back(ucs4);
    } else {
      temp.push_back(wstr[prev]);
      temp.push_back(wstr[prev + 1]);
    }
  }
  file_name->swap(temp);
#elif defined(WCHAR_T_IS_UTF32)
  while (i < length) {
    if (Singleton<IllegalCharacters>()->contains(wstr[i])) {
      (*file_name)[i] = replace_char;
    }
    ++i;
  }
#else
#error wchar_t* should be either UTF-16 or UTF-32
#endif
}

}  // namespace
