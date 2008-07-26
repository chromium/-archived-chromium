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

#include "chrome/browser/printing/units.h"

#include "base/logging.h"

namespace printing {

int ConvertUnit(int value, int old_unit, int new_unit) {
  DCHECK_GT(new_unit, 0);
  DCHECK_GT(old_unit, 0);
  // With integer arithmetic, to divide a value with correct rounding, you need
  // to add half of the divisor value to the dividend value. You need to do the
  // reverse with negative number.
  if (value >= 0) {
    return ((value * new_unit) + (old_unit / 2)) / old_unit;
  } else {
    return ((value * new_unit) - (old_unit / 2)) / old_unit;
  }
}

double ConvertUnitDouble(double value, double old_unit, double new_unit) {
  DCHECK_GT(new_unit, 0);
  DCHECK_GT(old_unit, 0);
  return value * new_unit / old_unit;
}

int ConvertMilliInchToHundredThousanthMeter(int milli_inch) {
  // 1" == 25.4 mm
  // 1" == 25400 um
  // 0.001" == 25.4 um
  // 0.001" == 2.54 cmm
  return ConvertUnit(milli_inch, 100, 254);
}

int ConvertHundredThousanthMeterToMilliInch(int cmm) {
  return ConvertUnit(cmm, 254, 100);
}

}  // namespace printing
