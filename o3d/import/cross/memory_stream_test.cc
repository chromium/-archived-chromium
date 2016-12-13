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


// Tests functionality of the MemoryReadStream and MemoryWriteStream
// classes

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/error.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"
#include "core/cross/types.h"

#if !defined(IS_LITTLE_ENDIAN) && !defined(IS_BIG_ENDIAN)
#error "IS_LITTLE_ENDIAN or IS_BIG_ENDIAN must be defined!"
#endif

namespace o3d {

static char *kTestString =
    "Tests functionality of the MemoryReadStream and MemoryWriteStream classes";

// Test fixture for MemoryReadStream and MemoryWriteStream.
class MemoryStreamTest : public testing::Test {
};

// Test reading from a buffer using MemoryReadStream
TEST_F(MemoryStreamTest, Read) {
  int i;

  // Put a test string in a memory buffer
  const int kStringLength = strlen(kTestString);
  MemoryBuffer<uint8> buffer(kStringLength);

  for (i = 0; i < kStringLength; ++i) {
    buffer[i] = kTestString[i];
  }

  // Now, create a read stream on that buffer and verify that it reads
  // correctly...
  MemoryReadStream read_stream(buffer, buffer.GetLength());

  EXPECT_EQ(buffer.GetLength(), read_stream.GetTotalStreamLength());
  EXPECT_EQ(kStringLength, read_stream.GetTotalStreamLength());

  // Read one byte at a time and verify
  uint8 c;
  for (i = 0; i < kStringLength; ++i) {
    c = read_stream.ReadByte();
    EXPECT_EQ(c, kTestString[i]);
  }
  // Read an extra byte and verify it's zero
  c = read_stream.ReadByte();
  EXPECT_EQ(c, 0);

  // Now, create a 2nd read stream
  MemoryReadStream read_stream2(buffer, buffer.GetLength());
  // Get direct memory access and check the pointer is correct
  const uint8 *p = read_stream2.GetDirectMemoryPointer();
  EXPECT_EQ(const_cast<uint8*>(p), static_cast<uint8*>(buffer));

  // Test the Read() method

  // First read 5 bytes
  MemoryBuffer<uint8> read_buffer(kStringLength);
  int bytes_read = read_stream2.Read(read_buffer, 5);

  // Verify bytes read, stream position and remaining byte count
  EXPECT_EQ(5, bytes_read);
  EXPECT_EQ(5, read_stream2.GetStreamPosition());
  EXPECT_EQ(kStringLength - 5, read_stream2.GetRemainingByteCount());

  // Next read the remaining bytes
  bytes_read = read_stream2.Read(read_buffer + 5, kStringLength - 5);
  EXPECT_EQ(kStringLength - 5, bytes_read);

  // Make sure we read the correct data
  EXPECT_EQ(0, memcmp(read_buffer, buffer, kStringLength));

  // Try to read some more, even though we're at stream end
  bytes_read = read_stream2.Read(read_buffer, 1000);
  EXPECT_EQ(0, bytes_read);  // make sure no bytes were read

  // Now, create a 3rd read stream
  MemoryReadStream read_stream3(buffer, buffer.GetLength());

  // Let's test the Skip() method
  read_stream3.Skip(6);  // skip over the first 6 bytes

  // Read three bytes in a row and verify
  uint8 c1 = read_stream3.ReadByte();
  uint8 c2 = read_stream3.ReadByte();
  uint8 c3 = read_stream3.ReadByte();
  EXPECT_EQ('f', c1);
  EXPECT_EQ('u', c2);
  EXPECT_EQ('n', c3);
}

// Test Writing to a buffer using MemoryWriteStream
TEST_F(MemoryStreamTest, Write) {
  // Create a write stream without assigning it to memory yet
  MemoryWriteStream empty_stream;

  // Verfify length is zero
  EXPECT_EQ(0, empty_stream.GetTotalStreamLength());

  // Now, assign it to the string (OK, we can't really write to
  // this memory, but we're just checking the API here
  const int kStringLength = strlen(kTestString);
  empty_stream.Assign(reinterpret_cast<uint8*>(kTestString), kStringLength);

  // Sanity check on length, position, remaining
  EXPECT_EQ(kStringLength, empty_stream.GetTotalStreamLength());
  EXPECT_EQ(0, empty_stream.GetStreamPosition());
  EXPECT_EQ(kStringLength, empty_stream.GetRemainingByteCount());

  // Create a write stream on a buffer we can write to
  MemoryBuffer<uint8> buffer(kStringLength);
  MemoryWriteStream write_stream(buffer, buffer.GetLength());
  EXPECT_EQ(buffer.GetLength(), kStringLength);

  // Write 5 bytes
  uint8 *p = reinterpret_cast<uint8*>(kTestString);
  int bytes_written = write_stream.Write(p, 5);
  EXPECT_EQ(5, bytes_written);
  EXPECT_EQ(5, write_stream.GetStreamPosition());
  EXPECT_EQ(kStringLength - 5, write_stream.GetRemainingByteCount());

  // Write the remaining bytes in the string
  bytes_written = write_stream.Write(p + 5, kStringLength - 5);
  EXPECT_EQ(kStringLength - 5, bytes_written);
  EXPECT_EQ(kStringLength, write_stream.GetStreamPosition());
  EXPECT_EQ(0, write_stream.GetRemainingByteCount());

  // Verify we wrote the correct data
  EXPECT_EQ(0, memcmp(buffer, kTestString, kStringLength));

  // Try to write some more even though the buffer is full
  bytes_written = write_stream.Write(p, kStringLength);
  EXPECT_EQ(0, bytes_written);
}

// Basic sanity check for endian writing/reading
TEST_F(MemoryStreamTest, EndianSanity16) {
  // Sanity check int16
  MemoryBuffer<int16> buffer16(2);
  int16 *p = buffer16;
  uint8 *p8 = reinterpret_cast<uint8*>(p);
  MemoryWriteStream write_stream(p8, sizeof(int16) * 2);

  int16 value = 0x1234;
  write_stream.WriteLittleEndianInt16(value);
  write_stream.WriteBigEndianInt16(value);

  // Verify that the bytes are in the correct order
  uint8 low_byte = value & 0xff;
  uint8 high_byte = value >> 8;

  // validate little-endian
  EXPECT_EQ(low_byte, p8[0]);
  EXPECT_EQ(high_byte, p8[1]);

  // validate big-endian
  EXPECT_EQ(high_byte, p8[2]);
  EXPECT_EQ(low_byte, p8[3]);
}

// Basic sanity check for endian writing/reading
TEST_F(MemoryStreamTest, EndianSanity32) {
  // Sanity check int32
  MemoryBuffer<int32> buffer32(2);
  int32 *p = buffer32;
  uint8 *p8 = reinterpret_cast<uint8*>(p);
  MemoryWriteStream write_stream(p8, sizeof(int32) * 2);

  int32 value = 0x12345678;
  write_stream.WriteLittleEndianInt32(value);
  write_stream.WriteBigEndianInt32(value);

  // Verify that the bytes are in the correct order
  uint8 byte1 = value & 0xff;
  uint8 byte2 = (value >> 8) & 0xff;
  uint8 byte3 = (value >> 16) & 0xff;
  uint8 byte4 = (value >> 24) & 0xff;

  // validate little-endian
  EXPECT_EQ(byte1, p8[0]);
  EXPECT_EQ(byte2, p8[1]);
  EXPECT_EQ(byte3, p8[2]);
  EXPECT_EQ(byte4, p8[3]);

  // validate big-endian
  EXPECT_EQ(byte4, p8[4]);
  EXPECT_EQ(byte3, p8[5]);
  EXPECT_EQ(byte2, p8[6]);
  EXPECT_EQ(byte1, p8[7]);
}

// Basic sanity check for endian writing/reading
TEST_F(MemoryStreamTest, EndianSanityFloat32) {
  // Sanity check int32
  MemoryBuffer<float> buffer32(2);
  float *p = buffer32;
  uint8 *p8 = reinterpret_cast<uint8*>(p);
  MemoryWriteStream write_stream(p8, sizeof(int32) * 2);

  float value = 3.14159f;
  write_stream.WriteLittleEndianFloat32(value);
  write_stream.WriteBigEndianFloat32(value);

  // Verify that the bytes are in the correct order
  int32 ivalue = *reinterpret_cast<int32*>(&value);  // interpret float as int32
  uint8 byte1 = ivalue & 0xff;
  uint8 byte2 = (ivalue >> 8) & 0xff;
  uint8 byte3 = (ivalue >> 16) & 0xff;
  uint8 byte4 = (ivalue >> 24) & 0xff;

  // validate little-endian
  EXPECT_EQ(byte1, p8[0]);
  EXPECT_EQ(byte2, p8[1]);
  EXPECT_EQ(byte3, p8[2]);
  EXPECT_EQ(byte4, p8[3]);

  // validate big-endian
  EXPECT_EQ(byte4, p8[4]);
  EXPECT_EQ(byte3, p8[5]);
  EXPECT_EQ(byte2, p8[6]);
  EXPECT_EQ(byte1, p8[7]);
}

// Write then read int16, int32, and float32 little/big endian values
TEST_F(MemoryStreamTest, Endian) {
  const int16 kValue1 = 13243;
  const int32 kValue2 = 2393043;
  const float kValue3 = -0.039483f;
  const int16 kValue4 = -3984;
  const float kValue5 = 1234.5678f;
  const uint8 kValue6 = 5;  // write a single byte to make things interesting
  const int32 kValue7 = -3920393;

  size_t total_size = sizeof(kValue1) +
                      sizeof(kValue2) +
                      sizeof(kValue3) +
                      sizeof(kValue4) +
                      sizeof(kValue5) +
                      sizeof(kValue6) +
                      sizeof(kValue7);

  MemoryBuffer<uint8> buffer(total_size);

  // write the values to the buffer
  uint8 *p8 = buffer;
  MemoryWriteStream write_stream(p8, total_size);

  write_stream.WriteLittleEndianInt16(kValue1);
  write_stream.WriteBigEndianInt32(kValue2);
  write_stream.WriteLittleEndianFloat32(kValue3);
  write_stream.WriteBigEndianInt16(kValue4);
  write_stream.WriteBigEndianFloat32(kValue5);
  write_stream.Write(&kValue6, 1);
  write_stream.WriteLittleEndianInt32(kValue7);

  // now read them back in and verify
  int16 read_value1;
  int32 read_value2;
  float read_value3;
  int16 read_value4;
  float read_value5;
  uint8 read_value6;
  int32 read_value7;
  MemoryReadStream read_stream(p8, total_size);

  read_value1 = read_stream.ReadLittleEndianInt16();
  read_value2 = read_stream.ReadBigEndianInt32();
  read_value3 = read_stream.ReadLittleEndianFloat32();
  read_value4 = read_stream.ReadBigEndianInt16();
  read_value5 = read_stream.ReadBigEndianFloat32();
  read_stream.Read(&read_value6, 1);
  read_value7 = read_stream.ReadLittleEndianInt32();

  // Validate
  EXPECT_EQ(read_value1, kValue1);
  EXPECT_EQ(read_value2, kValue2);
  EXPECT_EQ(read_value3, kValue3);
  EXPECT_EQ(read_value4, kValue4);
  EXPECT_EQ(read_value5, kValue5);
  EXPECT_EQ(read_value6, kValue6);
  EXPECT_EQ(read_value7, kValue7);
}

}  // namespace o3d
