// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RAND_UTIL_H__
#define CHROME_COMMON_RAND_UTIL_H__

#include "base/basictypes.h"

namespace rand_util {

// Returns a random number between min and max (inclusive). This is a
// non-cryptographic random number generator, using rand() from the CRT.
int RandInt(int min, int max);

// Returns a random number between min and max (inclusive). This is a (slower)
// cryptographic random number generator using rand_s() from the CRT.  Note
// that it does not work in Win2K, so it degrades to RandInt.
int RandIntSecure(int min, int max);

}  // namespace rand_util

#endif // CHROME_COMMON_RAND_UTIL_H__

