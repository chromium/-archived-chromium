// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/streams.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(StreamsTest, SimpleWriteRead) {
  const unsigned int kValue1 = 12345;
  courgette::SinkStream sink;

  sink.WriteVarint32(kValue1);

  const uint8* sink_buffer = sink.Buffer();
  size_t length = sink.Length();

  courgette::SourceStream source;
  source.Init(sink_buffer, length);

  unsigned int value;
  bool can_read = source.ReadVarint32(&value);
  EXPECT_EQ(true, can_read);
  EXPECT_EQ(kValue1, value);
  EXPECT_EQ(0, source.Remaining());
}

TEST(StreamsTest, SimpleWriteRead2) {
  courgette::SinkStream sink;

  sink.Write("Hello", 5);

  const uint8* sink_buffer = sink.Buffer();
  size_t sink_length = sink.Length();

  courgette::SourceStream source;
  source.Init(sink_buffer, sink_length);

  char text[10] = {0};
  bool can_read = source.Read(text, 5);
  EXPECT_EQ(true, can_read);
  EXPECT_EQ(0, memcmp("Hello", text, 5));
  EXPECT_EQ(0, source.Remaining());
}

TEST(StreamsTest, StreamSetWriteRead) {
  courgette::SinkStreamSet out;
  out.Init(4);

  const unsigned int kValue1 = 12345;

  out.stream(3)->WriteVarint32(kValue1);

  courgette::SinkStream collected;

  out.CopyTo(&collected);

  const uint8* collected_buffer = collected.Buffer();
  size_t collected_length = collected.Length();

  courgette::SourceStreamSet in;
  bool can_init = in.Init(collected_buffer, collected_length);
  EXPECT_EQ(true, can_init);

  uint32 value;
  bool can_read = in.stream(3)->ReadVarint32(&value);
  EXPECT_EQ(true, can_read);
  EXPECT_EQ(kValue1, value);
  EXPECT_EQ(0, in.stream(3)->Remaining());
  EXPECT_EQ(0, in.stream(2)->Remaining());
}

TEST(StreamsTest, StreamSetWriteRead2) {
  const size_t kNumberOfStreams = 4;
  const unsigned int kEnd = ~0U;

  courgette::SinkStreamSet out;
  out.Init(kNumberOfStreams);

  static const unsigned int data[] = {
    3, 123,  3, 1000,  0, 100, 2, 100,  0, 999999,
    0, 0,  0, 0,  1, 2,  1, 3,  1, 5,  0, 66,
    // varint32 edge case values:
    1, 127,  1, 128,  1, 129,  1, 16383,  1, 16384,
    kEnd
  };

  for (size_t i = 0;  data[i] != kEnd;  i += 2) {
    size_t id = data[i];
    size_t datum = data[i + 1];
    out.stream(id)->WriteVarint32(datum);
  }

  courgette::SinkStream collected;

  out.CopyTo(&collected);

  courgette::SourceStreamSet in;
  bool can_init = in.Init(collected.Buffer(), collected.Length());
  EXPECT_EQ(true, can_init);

  for (size_t i = 0;  data[i] != kEnd;  i += 2) {
    size_t id = data[i];
    size_t datum = data[i + 1];
    uint32 value = 77;
    bool can_read = in.stream(id)->ReadVarint32(&value);
    EXPECT_EQ(true, can_read);
    EXPECT_EQ(datum, value);
  }

  for (size_t i = 0;  i < kNumberOfStreams;  ++i) {
    EXPECT_EQ(0, in.stream(i)->Remaining());
  }
}

TEST(StreamsTest, SignedVarint32) {
  courgette::SinkStream out;

  static const int32 data[] = {
    0, 64, 128, 8192, 16384,
    1 << 20, 1 << 21, 1 << 22,
    1 << 27, 1 << 28,
    0x7fffffff, -0x7fffffff
  };

  std::vector<int32> values;
  for (size_t i = 0;  i < sizeof(data)/sizeof(data[0]);  ++i) {
    int32 basis = data[i];
    for (int delta = -4; delta <= 4; ++delta) {
      out.WriteVarint32Signed(basis + delta);
      values.push_back(basis + delta);
      out.WriteVarint32Signed(-basis + delta);
      values.push_back(-basis + delta);
    }
  }

  courgette::SourceStream in;
  in.Init(out);

  for (size_t i = 0;  i < values.size();  ++i) {
    int written_value = values[i];
    int32 datum;
    bool can_read = in.ReadVarint32Signed(&datum);
    EXPECT_EQ(written_value, datum);
  }

  EXPECT_EQ(true, in.Empty());
}

TEST(StreamsTest, StreamSetReadWrite) {
  courgette::SinkStreamSet out;

  { // Local scope for temporary stream sets.
    courgette::SinkStreamSet subset1;
    subset1.stream(3)->WriteVarint32(30000);
    subset1.stream(5)->WriteVarint32(50000);
    out.WriteSet(&subset1);

    courgette::SinkStreamSet subset2;
    subset2.stream(2)->WriteVarint32(20000);
    subset2.stream(6)->WriteVarint32(60000);
    out.WriteSet(&subset2);
  }

  courgette::SinkStream collected;
  out.CopyTo(&collected);
  courgette::SourceStreamSet in;
  bool can_init_in = in.Init(collected.Buffer(), collected.Length());
  EXPECT_EQ(true, can_init_in);

  courgette::SourceStreamSet subset1;
  bool can_read_1 = in.ReadSet(&subset1);
  EXPECT_EQ(true, can_read_1);
  EXPECT_EQ(false, in.Empty());

  courgette::SourceStreamSet subset2;
  bool can_read_2 = in.ReadSet(&subset2);
  EXPECT_EQ(true, can_read_2);
  EXPECT_EQ(true, in.Empty());

  courgette::SourceStreamSet subset3;
  bool can_read_3 = in.ReadSet(&subset3);
  EXPECT_EQ(false, can_read_3);

  EXPECT_EQ(false, subset1.Empty());
  EXPECT_EQ(false, subset1.Empty());

  uint32 datum;
  EXPECT_EQ(true, subset1.stream(3)->ReadVarint32(&datum));
  EXPECT_EQ(30000, datum);
  EXPECT_EQ(true, subset1.stream(5)->ReadVarint32(&datum));
  EXPECT_EQ(50000, datum);
  EXPECT_EQ(true, subset1.Empty());

  EXPECT_EQ(true, subset2.stream(2)->ReadVarint32(&datum));
  EXPECT_EQ(20000, datum);
  EXPECT_EQ(true, subset2.stream(6)->ReadVarint32(&datum));
  EXPECT_EQ(60000, datum);
  EXPECT_EQ(true, subset2.Empty());
}
