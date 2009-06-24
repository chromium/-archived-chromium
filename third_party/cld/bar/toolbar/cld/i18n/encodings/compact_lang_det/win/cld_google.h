// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_GOOGLE_H_
#define BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_GOOGLE_H_

// Nothing to be included for windows version.
// For now, comment out CLD_WINDOWS since it is leading to some problems
// with unit testing, because google.h has been removed from the trunk.
// When linux/mac versions are implemented, google.h (and associated files)
// can be checked in as required.
/*
#if !defined(CLD_WINDOWS)

#include "third_party/cld/base/google.h"

#else

// Include nothing

#endif
*/
#endif  // BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_GOOGLE_H_
