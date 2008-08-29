// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/rand_util.h"

#include <stdlib.h>

#include "base/logging.h"

namespace rand_util {

int RandInt(int min, int max) {
  return RandIntSecure(min, max);
}

int RandIntSecure(int min, int max) {
  unsigned int number;
  // This code will not work on win2k, which we do not support.
  errno_t rv = rand_s(&number);
  DCHECK(rv == 0) << "rand_s failed with error " << rv;

  // From the rand man page, use this instead of just rand() % max, so that the
  // higher bits are used.
  return min + static_cast<int>(static_cast<double>(max - min + 1.0) *
      (number / (UINT_MAX + 1.0)));
}

}  // namespace rand_util
