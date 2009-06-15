// Copyright 2009 Google Inc.  All Rights Reserved.
// Author: alekseys@google.com (Aleksey Shlyapnikov)

// This code is not actually used, it was copied here for the reference only.
// See cld_htmlutils_windows.cc for Windows version of this code.

#include "cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_unilib.h"

#include "cld/util/utf8/unilib.h"

namespace cld_UniLib {

int OneCharLen(const char* src) {
  return UniLib::OneCharLen(src);
}

}  // namespace cld_UniLib
