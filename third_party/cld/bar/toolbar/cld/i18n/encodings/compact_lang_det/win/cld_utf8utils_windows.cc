// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8utils.h"

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8statetable.h"

namespace cld {

int UTF8GenericScan(const UTF8ScanObj* st, const char* src, int len,
                    int* bytes_consumed) {
  return ::UTF8GenericScan(st, reinterpret_cast<const uint8*>(src), len,
                           bytes_consumed);
}

}  // namespace cld
