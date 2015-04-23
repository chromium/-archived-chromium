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


// Tests functionality of the MemoryBuffer class

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/error.h"
#include "import/cross/memory_buffer.h"

namespace o3d {

// Test fixture for RawData testing.
class MemoryBufferTest : public testing::Test {
};

// Test RawData
TEST_F(MemoryBufferTest, Basic) {
  int i;
  MemoryBuffer<int> buffer;
  // Check that initially the buffer is not allocated
  ASSERT_EQ(0, buffer.GetLength());

  // Allocate and check the length is good
  const int kBufferLength = 1024;
  buffer.Allocate(kBufferLength);
  ASSERT_EQ(kBufferLength, buffer.GetLength());

  // Once allocated, the initial contents should be zero
  // Check that the buffer contents are zeroed out
  bool buffer_is_cleared = true;
  for (i = 0; i < kBufferLength; ++i) {
    if (buffer[i] != 0) {
      buffer_is_cleared = false;
      break;
    }
  }
  ASSERT_TRUE(buffer_is_cleared);

  // Write some values and check that they're OK
  for (i = 0; i < kBufferLength; ++i) {
    buffer[i] = i;
  }
  bool buffer_values_good = true;
  for (i = 0; i < kBufferLength; ++i) {
    if (buffer[i] != i) {
      buffer_values_good = false;
      break;
    }
  }
  ASSERT_TRUE(buffer_values_good);

  // Now, clear the buffer and check that it worked
  buffer.Clear();
  buffer_is_cleared = true;
  for (i = 0; i < kBufferLength; ++i) {
    if (buffer[i] != 0) {
      buffer_is_cleared = false;
      break;
    }
  }
  ASSERT_TRUE(buffer_is_cleared);

  // Deallocate the buffer and verify its length
  buffer.Deallocate();
  ASSERT_EQ(0, buffer.GetLength());
}

}  // namespace o3d
