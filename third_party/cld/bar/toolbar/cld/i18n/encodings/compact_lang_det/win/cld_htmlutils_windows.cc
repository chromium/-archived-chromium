// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_htmlutils.h"

// Src points to '&'
// Writes entity value to dst. Returns take(src), put(dst) byte counts
void EntityToBuffer(const char* src, int len, char* dst,
                    int* tlen, int* plen) {
  // On Windows we do not have to do anything, browser expands HTML entities
  // for us, so text we're retrieving from it is ready for translation as it is.
  // But:

  // This is a temporary solution to let us continue the development without
  // having a real DOM text scraping in place.  For now the full HTML is fed
  // to CLD for language detection and just ignoring entities is good enough
  // for testing.  Later entities will be expanded by browser itself.

  // Skip entity in the source.
  *tlen = 1;
  do {
    ++src;
    ++*tlen;
  } while (*src && *src != ';');
  // Report a bogus entity (space).
  *dst = ' ';
  *plen = 1;
}
