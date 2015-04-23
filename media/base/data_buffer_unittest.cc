// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "media/base/data_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::DataBuffer;

TEST(DataBufferTest, Basic) {
  const char kData[] = "hello";
  const size_t kDataSize = arraysize(kData);
  const char kNewData[] = "chromium";
  const size_t kNewDataSize = arraysize(kNewData);
  const base::TimeDelta kTimestampA = base::TimeDelta::FromMicroseconds(1337);
  const base::TimeDelta kDurationA = base::TimeDelta::FromMicroseconds(1667);
  const base::TimeDelta kTimestampB = base::TimeDelta::FromMicroseconds(1234);
  const base::TimeDelta kDurationB = base::TimeDelta::FromMicroseconds(5678);

  // Create our buffer and copy some data into it.
  // Create a DataBuffer.
  scoped_refptr<DataBuffer> buffer = new DataBuffer();
  ASSERT_TRUE(buffer);

  // Test StreamSample implementation.
  buffer->SetTimestamp(kTimestampA);
  buffer->SetDuration(kDurationA);
  EXPECT_TRUE(kTimestampA == buffer->GetTimestamp());
  EXPECT_TRUE(kDurationA == buffer->GetDuration());
  EXPECT_TRUE(buffer->IsEndOfStream());
  EXPECT_FALSE(buffer->GetData());
  EXPECT_FALSE(buffer->IsDiscontinuous());
  buffer->SetTimestamp(kTimestampB);
  buffer->SetDuration(kDurationB);
  EXPECT_TRUE(kTimestampB == buffer->GetTimestamp());
  EXPECT_TRUE(kDurationB == buffer->GetDuration());

  buffer->SetDiscontinuous(true);
  EXPECT_TRUE(buffer->IsDiscontinuous());
  buffer->SetDiscontinuous(false);
  EXPECT_FALSE(buffer->IsDiscontinuous());

  // Test WritableBuffer implementation.
  EXPECT_FALSE(buffer->GetData());
  uint8* data = buffer->GetWritableData(kDataSize);
  ASSERT_TRUE(data);
  ASSERT_EQ(buffer->GetDataSize(), kDataSize);
  memcpy(data, kData, kDataSize);
  const uint8* read_only_data = buffer->GetData();
  ASSERT_EQ(data, read_only_data);
  ASSERT_EQ(0, memcmp(read_only_data, kData, kDataSize));
  EXPECT_FALSE(buffer->IsEndOfStream());

  data = buffer->GetWritableData(kNewDataSize + 10);
  ASSERT_TRUE(data);
  ASSERT_EQ(buffer->GetDataSize(), kNewDataSize + 10);
  memcpy(data, kNewData, kNewDataSize);
  read_only_data = buffer->GetData();
  buffer->SetDataSize(kNewDataSize);
  EXPECT_EQ(buffer->GetDataSize(), kNewDataSize);
  ASSERT_EQ(data, read_only_data);
  EXPECT_EQ(0, memcmp(read_only_data, kNewData, kNewDataSize));
}
