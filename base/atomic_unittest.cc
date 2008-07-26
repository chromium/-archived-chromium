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

#include "base/atomic.h"

#include "base/lock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(AtomicTest, Increment) {
  int32 value = 38;
  int32 new_value = base::AtomicIncrement(&value);
  EXPECT_EQ(39, value);
  EXPECT_EQ(39, new_value);
}

TEST(AtomicTest, Decrement) {
  int32 value = 49;
  int32 new_value = base::AtomicDecrement(&value);
  EXPECT_EQ(48, value);
  EXPECT_EQ(48, new_value);
}

TEST(AtomicTest, Swap) {
  int32 value = 38;
  int32 old_value = base::AtomicSwap(&value, 49);
  EXPECT_EQ(49, value);
  EXPECT_EQ(38, old_value);

  // Now do another test.
  value = 0;
  old_value = base::AtomicSwap(&value, 1);
  EXPECT_EQ(0, old_value);
  old_value = base::AtomicSwap(&value, 1);
  EXPECT_EQ(1, old_value);
  old_value = base::AtomicSwap(&value, 1);
  EXPECT_EQ(1, old_value);
}
