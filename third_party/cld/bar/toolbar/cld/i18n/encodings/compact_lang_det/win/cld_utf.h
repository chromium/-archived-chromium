// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_UTF_H_
#define BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_UTF_H_

#if !defined(CLD_WINDOWS)

//#include "third_party/utf/utf.h"

#else

enum {
  UTFmax        = 4,            // maximum bytes per rune
  Runesync      = 0x80,         // cannot represent part of a UTF sequence (<)
  Runeself      = 0x80,         // rune and UTF sequences are the same (<)
  Runeerror     = 0xFFFD,       // decoding error in UTF
  Runemax       = 0x10FFFF,     // maximum rune value
};

#endif

#endif  // BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_UTF_H_
