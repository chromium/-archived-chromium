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
#include <string>

#include "core/cross/client.h"
#include "import/cross/memory_stream.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/gz_decompressor.h"
#include "import/cross/gz_compressor.h"
#include "tests/common/win/testing_common.h"
#include "tests/common/cross/test_utils.h"

namespace o3d {

class GzCompressorTest : public testing::Test {
};

using o3d::MemoryReadStream;
using o3d::MemoryWriteStream;
using o3d::GzCompressor;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class for receiving the decompressed byte stream
class DecompressorClient : public o3d::StreamProcessor {
 public:
  explicit DecompressorClient(size_t uncompressed_byte_size)
      : buffer_(uncompressed_byte_size),
        write_stream_(buffer_, uncompressed_byte_size) {}

  virtual int     ProcessBytes(MemoryReadStream *stream,
                               size_t bytes_to_process) {
    // Make sure the output stream isn't full yet
    EXPECT_TRUE(write_stream_.GetRemainingByteCount() >= bytes_to_process);

    // Buffer the decompressed bytes
    const uint8 *p = stream->GetDirectMemoryPointer();
    stream->Skip(bytes_to_process);
    write_stream_.Write(p, bytes_to_process);

    return 0;  // return OK
  }

  void  VerifyDecompressedOutput(uint8 *original_data) {
    // Make sure we filled up the output buffer
    EXPECT_EQ(0, write_stream_.GetRemainingByteCount());

    // Verify decompressed data with the original data we fed into the
    // compressor
    EXPECT_EQ(0, memcmp(original_data, buffer_, buffer_.GetLength()));
  }

 private:
  size_t uncompressed_byte_size_;
  MemoryBuffer<uint8> buffer_;
  MemoryWriteStream write_stream_;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class for receiving the compressed byte stream
class CompressorClient : public o3d::StreamProcessor {
 public:
  explicit CompressorClient(size_t uncompressed_byte_size)
      : decompressor_client_(uncompressed_byte_size),
        decompressor_(&decompressor_client_) {
  };

  virtual int     ProcessBytes(MemoryReadStream *stream,
                               size_t bytes_to_process) {
    // We're receiving compressed bytes here, so feed them back into
    // the decompressor.  Since we're making a compression / decompression
    // round trip, we should end up with the same (initial) byte stream
    // we can verify this at the end
    int result = decompressor_.ProcessBytes(stream, bytes_to_process);

    // Verify the result code:
    //  zlib FAQ says Z_BUF_ERROR is OK
    //  and may occur in the middle of decompressing the stream
    EXPECT_TRUE(result == Z_OK || result == Z_STREAM_END ||
                result == Z_BUF_ERROR);

    return 0;  // no error
  }

  void  VerifyDecompressedOutput(uint8 *original_data) {
    // Send it on to the client, since its the one with the buffered data
    decompressor_client_.VerifyDecompressedOutput(original_data);
  }

 private:
  DecompressorClient decompressor_client_;
  o3d::GzDecompressor decompressor_;
};



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// int main (int argc, const char * argv[]) {
TEST_F(GzCompressorTest, RoundTripCompressionDecompression) {
  // We'll compress this file
  String filepath = *g_program_path + "/archive_files/keyboard.jpg";

  size_t file_length;
  uint8 *p = test_utils::ReadFile(filepath.c_str(), &file_length);
  ASSERT_TRUE(p != NULL);

  MemoryReadStream input_file_stream(p, file_length);

  // Gets callbacks with compressed bytes
  CompressorClient compressor_client(file_length);

  GzCompressor compressor(&compressor_client);

  // Compress the entire file contents.
  // |compressor_client| will take the compressed byte stream and send it
  // directly to a decompressor.
  //
  // Since we're making a compression / decompression
  // round trip, we should end up with the same (initial) byte stream
  // we can verify this at the end by calling VerifyDecompressedOutput()
  int result = compressor.ProcessBytes(&input_file_stream, file_length);
  EXPECT_TRUE(result == Z_OK || result == Z_STREAM_END);

  compressor.Finalize();

  compressor_client.VerifyDecompressedOutput(p);

  free(p);
}

}  // namespace o3d
