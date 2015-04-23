// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Author: alekseys@google.com (Aleksey Shlyapnikov)

// This code is not actually used, it was copied here for the reference only.
// See cld_htmlutils_windows.cc for Windows version of this code.

#include "cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_htmlutils.h"

#include "cld/third_party/utf/utf.h"        // for runetochar
#include "cld/webutil/html/htmlutils.h"     // for ReadEntity

// Copied from getonescriptspan.cc

// Src points to '&'
// Writes entity value to dst. Returns take(src), put(dst) byte counts
void EntityToBuffer(const char* src, int len, char* dst,
                    int* tlen, int* plen) {
  char32 entval = HtmlUtils::ReadEntity(src, len, tlen);
  // ReadEntity does this already: entval = FixUnicodeValue(entval);

  if (entval > 0) {
    *plen = runetochar(dst, &entval);
  } else {
    // Illegal entity; ignore the '&'
    *tlen = 1;
    *plen = 0;
  }
  // fprintf(stderr,"t%d p%d]\n", *tlen, *plen);
}
