// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_UTF8UTILS_H_
#define BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_UTF8UTILS_H_

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8statetable.h"

namespace cld {

// Scan a UTF-8 stringpiece based on a state table.
// Always scan complete UTF-8 characters
// Set number of bytes scanned. Return reason for exiting
int UTF8GenericScan(const UTF8ScanObj* st,
                    const char* src,
                    int len,
                    int* bytes_consumed);

}  // namespace cld

#endif  // BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_UTF8UTILS_H_
