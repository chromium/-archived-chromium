/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef O3D_STATSREPORT_AGGREGATOR_WIN32_UNITTEST_H__
#define O3D_STATSREPORT_AGGREGATOR_WIN32_UNITTEST_H__

#include "aggregator_unittest.h"
#include "aggregator-win32.h"

// Shared test fixture for win32 unit tests
class MetricsAggregatorWin32Test: public MetricsAggregatorTest {
 public:
  virtual void SetUp() {
    // clean the registry
    SHDeleteKey(HKEY_CURRENT_USER, kRootKeyName);
    MetricsAggregatorTest::SetUp();
  }
  virtual void TearDown() {
    MetricsAggregatorTest::TearDown();
    SHDeleteKey(HKEY_CURRENT_USER, kRootKeyName);
  }

  void AddStats() {
    ++c1_;
    ++c2_;
    ++c2_;

    t1_.AddSample(1000);
    t1_.AddSample(500);

    t2_.AddSample(2000);
    t2_.AddSample(30);

    i1_ = 1;
    i2_ = 2;

    b1_ = true;
    b2_ = false;
  }

  static const wchar_t kAppName[];
  static const wchar_t kRootKeyName[];
  static const wchar_t kCountsKeyName[];
  static const wchar_t kTimingsKeyName[];
  static const wchar_t kIntegersKeyName[];
  static const wchar_t kBoolsKeyName[];
};

#endif  // O3D_STATSREPORT_AGGREGATOR_WIN32_UNITTEST_H__
