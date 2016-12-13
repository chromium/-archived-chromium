// Copyright 2009 Google Inc.  All Rights Reserved.
// Author: alekseys@google.com (Aleksey Shlyapnikov)

// This code is not actually used, it was copied here for the reference only.
// See cld_htmlutils_windows.cc for Windows version of this code.

#include "cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8utils.h"

#include "cld/util/utf8/utf8statetable.h"

namespace cld {

int UTF8GenericScan(const UTF8ScanObj* st, const char* src, int len,
                    int* bytes_consumed) {
  return ::UTF8GenericScan(st, StringPiece(src, len), bytes_consumed);
}

}  // namespace cld
