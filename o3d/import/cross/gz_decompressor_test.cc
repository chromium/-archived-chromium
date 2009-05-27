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


#include <sys/stat.h>
#include <algorithm>

#include "core/cross/client.h"
#include "import/cross/gz_decompressor.h"
#include "import/cross/memory_buffer.h"
#include "tests/common/win/testing_common.h"
#include "tests/common/cross/test_utils.h"

using test_utils::ReadFile;

namespace o3d {

class GzDecompressorTest : public testing::Test {
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Receives bytes from the decompressor and buffers them
//
class GzTestClient : public StreamProcessor {
 public:
  explicit GzTestClient(size_t uncompressed_size) {
    buffer_.Allocate(uncompressed_size);
    stream_.Assign(buffer_, uncompressed_size);
  }

  virtual int     ProcessBytes(MemoryReadStream *input_stream,
                               size_t bytes_to_process) {
    // Buffer the uncompressed bytes we're given
    const uint8 *p = input_stream->GetDirectMemoryPointer();
    input_stream->Skip(bytes_to_process);

    size_t bytes_written = stream_.Write(p, bytes_to_process);
    EXPECT_EQ(bytes_written, bytes_to_process);

    return Z_OK;
  }

  // When we're done decompressing, we can check the results here
  uint8 *GetResultBuffer() { return buffer_; }
  size_t GetResultLength() const { return stream_.GetStreamPosition(); }

 private:
  MemoryBuffer<uint8> buffer_;
  MemoryWriteStream stream_;
};

// Loads a tar.gz file, runs it through the processor.
// In our callbacks, we verify that we receive three files with known contents
//
TEST_F(GzDecompressorTest, LoadGzFile) {
  String compressed_file = *g_program_path + "/archive_files/keyboard.jpg.gz";
  String uncompressed_file = *g_program_path + "/archive_files/keyboard.jpg";

  // Read the compressed and uncompressed files into memory.
  // We can then run the decompressor and check it against the expected
  // uncompressed data...
  size_t compressed_size;
  size_t uncompressed_size;
  uint8 *compressed_data = ReadFile(compressed_file, &compressed_size);
  uint8 *expected_uncompressed_data =
      ReadFile(uncompressed_file, &uncompressed_size);

  ASSERT_TRUE(compressed_data != NULL);
  ASSERT_TRUE(expected_uncompressed_data != NULL);

  // Gets callbacks for the uncompressed data
  GzTestClient decompressor_client(uncompressed_size);

  // The class we're testing...
  GzDecompressor decompressor(&decompressor_client);

  // Now that we've read the compressed file into memory, lets
  // feed it, a chunk at a time, into the decompressor
  const int kChunkSize = 512;

  MemoryReadStream compressed_stream(compressed_data, compressed_size);
  size_t bytes_to_process = compressed_size;

  int result = Z_OK;
  while (bytes_to_process > 0) {
    size_t bytes_this_time =
        bytes_to_process < kChunkSize ? bytes_to_process : kChunkSize;

    result = decompressor.ProcessBytes(&compressed_stream, bytes_this_time);
    EXPECT_TRUE(result == Z_OK || result == Z_STREAM_END);

    bytes_to_process -= bytes_this_time;
  }

  // When the decompressor has finished it should return Z_STREAM_END
  EXPECT_TRUE(result == Z_STREAM_END);

  // Now let's verify that what we just decompressed matches exactly
  // what's in the reference file...

  // First check that the lengths match
  EXPECT_EQ(decompressor_client.GetResultLength(), uncompressed_size);

  // Now check the data
  result = memcmp(decompressor_client.GetResultBuffer(),
                  expected_uncompressed_data,
                  uncompressed_size);
  EXPECT_EQ(0, result);

  free(compressed_data);
  free(expected_uncompressed_data);
}

}  // namespace o3d
