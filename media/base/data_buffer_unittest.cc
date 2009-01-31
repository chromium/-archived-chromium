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
  const base::TimeDelta kTimestampA = base::TimeDelta::FromMicroseconds(1337);
  const base::TimeDelta kDurationA = base::TimeDelta::FromMicroseconds(1667);
  const base::TimeDelta kTimestampB = base::TimeDelta::FromMicroseconds(1234);
  const base::TimeDelta kDurationB = base::TimeDelta::FromMicroseconds(5678);

  // Create our buffer and copy some data into it.
  char* data = new char[kBufferSize];
  ASSERT_TRUE(data);
  size_t copied = base::strlcpy(data, kData, kBufferSize);
  EXPECT_EQ(kDataSize, copied + 1);

  // Create a DataBuffer.
  scoped_refptr<DataBuffer> buffer;
  buffer = new DataBuffer(data, kBufferSize, kDataSize,
                          kTimestampA, kDurationA);
  ASSERT_TRUE(buffer.get());

  // Test StreamSample implementation.
  EXPECT_TRUE(kTimestampA == buffer->GetTimestamp());
  EXPECT_TRUE(kDurationA == buffer->GetDuration());
  buffer->SetTimestamp(kTimestampB);
  buffer->SetDuration(kDurationB);
  EXPECT_TRUE(kTimestampB == buffer->GetTimestamp());
  EXPECT_TRUE(kDurationB == buffer->GetDuration());

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
