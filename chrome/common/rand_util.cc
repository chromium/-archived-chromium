// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
