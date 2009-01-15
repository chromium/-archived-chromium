// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "media/base/data_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::DataBuffer;

TEST(DataBufferTest, Basic) {
  const size_t kBufferSize = 32;
  const char kData[] = "hello";
  const size_t kDataSize = arraysize(kData);
  const char kNewData[] = "chromium";
  const size_t kNewDataSize = arraysize(kNewData);

  // Create our buffer and copy some data into it.
  char* data = new char[kBufferSize];
  ASSERT_TRUE(data);
  size_t copied = base::strlcpy(data, kData, kBufferSize);
  EXPECT_EQ(kDataSize, copied + 1);

  // Create a DataBuffer.
  scoped_refptr<DataBuffer> buffer;
  buffer = new DataBuffer(data, kBufferSize, kDataSize, 1337, 1667);  
  ASSERT_TRUE(buffer.get());

  // Test StreamSample implementation.
  EXPECT_EQ(1337, buffer->GetTimestamp());
  EXPECT_EQ(1667, buffer->GetDuration());
  buffer->SetTimestamp(1234);
  buffer->SetDuration(5678);
  EXPECT_EQ(1234, buffer->GetTimestamp());
  EXPECT_EQ(5678, buffer->GetDuration());

  // Test Buffer implementation.
  ASSERT_EQ(data, buffer->GetData());
  EXPECT_EQ(kDataSize, buffer->GetDataSize());
  EXPECT_STREQ(kData, buffer->GetData());

  // Test WritableBuffer implementation.
  ASSERT_EQ(data, buffer->GetWritableData());
  EXPECT_EQ(kBufferSize, buffer->GetBufferSize());
  copied = base::strlcpy(data, kNewData, kBufferSize);
  EXPECT_EQ(kNewDataSize, copied + 1);
  buffer->SetDataSize(kNewDataSize);
  EXPECT_EQ(kNewDataSize, buffer->GetDataSize());
}
