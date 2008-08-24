// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/rand_util.h"

#include <stdlib.h>
#include <time.h>

#include "base/logging.h"
#include "base/thread_local_storage.h"
#include "base/time.h"
#include "base/win_util.h"

namespace rand_util {

// TODO(evanm): don't rely on static initialization.
// Using TLS since srand() needs to be called once in each thread.
TLSSlot g_tls_index;

int RandInt(int min, int max) {
  if (g_tls_index.Get() == 0) {
    g_tls_index.Set(reinterpret_cast<void*>(1));
    TimeDelta now = TimeTicks::UnreliableHighResNow() - TimeTicks();
    unsigned int seed = static_cast<unsigned int>(now.InMicroseconds());
    srand(seed);
  }

  // From the rand man page, use this instead of just rand() % max, so that the
  // higher bits are used.
  return min + static_cast<int>(static_cast<double>(max - min + 1) *
    (::rand() / (RAND_MAX + 1.0)));
}

int RandIntSecure(int min, int max) {
  if (win_util::GetWinVersion() < win_util::WINVERSION_XP) {
    // rand_s needs XP and later.
    return RandInt(min, max);
  }

  unsigned int number;
  errno_t rv = rand_s(&number);
  DCHECK(rv == 0) << "rand_s failed with error " << rv;

  // From the rand man page, use this instead of just rand() % max, so that the
  // higher bits are used.
  return min + static_cast<int>(static_cast<double>(max - min + 1.0) *
      (number / (UINT_MAX + 1.0)));
}

}  // namespace rand_util

