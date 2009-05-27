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


#include <string>

#include "base/file_util.h"
#include "core/cross/client.h"
#include "import/cross/memory_stream.h"
#include "import/cross/targz_generator.h"
#include "tests/common/cross/test_utils.h"
#include "tests/common/win/testing_common.h"
#include "utils/cross/file_path_utils.h"

// Define to generate a new golden file for the archive test.  Don't
// leave this enabled, as it may affect the results of the test.
#undef GENERATE_GOLDEN

namespace o3d {

class TarGzGeneratorTest : public testing::Test {
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class TarGzTestClient : public StreamProcessor {
 public:
  explicit TarGzTestClient(size_t reference_size)
      : compressed_data_(reference_size),
        write_stream_(compressed_data_, reference_size) {
  };

  virtual int     ProcessBytes(MemoryReadStream *stream,
                               size_t bytes_to_process) {
    // Simply buffer up the tar.gz bytes
    // When we've gotten them all our Validate() method will be called
    const uint8 *p = stream->GetDirectMemoryPointer();
    stream->Skip(bytes_to_process);

    size_t remaining = write_stream_.GetRemainingByteCount();
    EXPECT_TRUE(bytes_to_process <= remaining);

    write_stream_.Write(p, bytes_to_process);

    return 0;
  }

  // Checks that the data from the reference tar.gz file matches the tar.gz
  // stream we just ge5nerated
  void            Validate(uint8 *reference_data) {
    uint8 *received_data = compressed_data_;

    // on Windows the platform field is different than our reference file
    received_data[9] = 3;  // Force platform in header to 'UNIX'.

    EXPECT_EQ(0, memcmp(reference_data,
                        received_data,
                        compressed_data_.GetLength()));
  }

#if defined(GENERATE_GOLDEN)
  char* received_data() {
    uint8 *data = compressed_data_;
    data[9] = 3;  // Force platform in header to 'UNIX'.
    return reinterpret_cast<char*>(data);
  }

  int received_data_length() {
    return compressed_data_.GetLength();
  }
#endif

 private:
  MemoryBuffer<uint8>  compressed_data_;
  MemoryWriteStream    write_stream_;

  DISALLOW_COPY_AND_ASSIGN(TarGzTestClient);
};

// Generates a tar.gz archive (in memory) containing three files (image, audio,
// shader) in three separate directories.  The generated archive is then
// compared with a reference tar.gz file which is known to be correct.
//
TEST_F(TarGzGeneratorTest, GenerateTarGz) {
  String targz_reference_file = *g_program_path + "/archive_files/test2.tar.gz";
  String image_file = *g_program_path + "/archive_files/keyboard.jpg";
  String audio_file = *g_program_path + "/archive_files/perc.aif";
  String shader_file = *g_program_path + "/archive_files/BumpReflect.fx";

  size_t targz_reference_size;
  size_t image_size;
  size_t audio_size;
  size_t shader_size;

  // Read the test files into memory
  uint8 *targz_data =
      test_utils::ReadFile(targz_reference_file, &targz_reference_size);
  uint8 *image_data = test_utils::ReadFile(image_file, &image_size);
  uint8 *audio_data = test_utils::ReadFile(audio_file, &audio_size);
  uint8 *shader_data = test_utils::ReadFile(shader_file, &shader_size);
  ASSERT_TRUE(targz_data != NULL);
  ASSERT_TRUE(image_data != NULL);
  ASSERT_TRUE(audio_data != NULL);
  ASSERT_TRUE(shader_data != NULL);

  ASSERT_NE(0, targz_reference_size);

  TarGzTestClient test_client(targz_reference_size);
  TarGzGenerator targz_generator(&test_client);

  targz_generator.AddFile("test/images/keyboard.jpg", image_size);
  targz_generator.AddFileBytes(image_data, image_size);

  targz_generator.AddFile("test/audio/perc.aif", audio_size);
  targz_generator.AddFileBytes(audio_data, audio_size);

  targz_generator.AddFile("test/shaders/BumpReflect.fx", shader_size);
  targz_generator.AddFileBytes(shader_data, shader_size);

  targz_generator.Finalize();

#if defined(GENERATE_GOLDEN)
  std::string new_golden_file = *g_program_path +
                                "/archive_files/new_golden_test2.tar.gz";
  file_util::WriteFile(UTF8ToFilePath(new_golden_file),
                       test_client.received_data(),
                       test_client.received_data_length());

#endif

  test_client.Validate(targz_data);

  free(targz_data);
  free(image_data);
  free(audio_data);
  free(shader_data);
}

}  // namespace o3d
