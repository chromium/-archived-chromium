// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <math.h>

#include "base/basictypes.h"
#include "base/logging.h"

namespace {

union uint64_splitter {
  uint64 normal;
  uint16 split[4];
};

}  // namespace

namespace base {

int RandInt(int min, int max) {
  DCHECK(min <= max);

  uint64 range = static_cast<int64>(max) - min + 1;
  uint64 number = base::RandUInt64();
  int result = min + static_cast<int>(number % range);
  DCHECK(result >= min && result <= max);
  return result;
}

double RandDouble() {
  uint64_splitter number;
  number.normal = base::RandUInt64();

  // Standard code based on drand48 would give only 48 bits of precision.
  // We try to get maximum precision for IEEE 754 double (52 bits).
  double result = ldexp(static_cast<double>(number.split[0] & 0xf), -52) +
                  ldexp(static_cast<double>(number.split[1]), -48) +
                  ldexp(static_cast<double>(number.split[2]), -32) +
                  ldexp(static_cast<double>(number.split[3]), -16);
  DCHECK(result >= 0.0 && result < 1.0);
  return result;
}

}  // namespace base
