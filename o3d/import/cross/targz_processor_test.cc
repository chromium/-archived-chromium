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


#include "core/cross/client.h"
#include "import/cross/targz_processor.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

class TarGzProcessorTest : public testing::Test {
};

// We verify that the tar.gz file contains exactly these filenames
const char *kFilename1 = "test/file1";
const char *kFilename2 = "test/file2";
const char *kFilename3 = "test/file3";

// With each file having these exact contents
#define kFileContents1 "the cat in the hat\n"
#define kFileContents2 "abracadabra\n"
#define kFileContents3 "I think therefore I am\n"

// we should receive these (and exactly these bytes in this order)
const char *kConcatenatedContents =
    kFileContents1  kFileContents2  kFileContents3;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class ArchiveTestClient : public ArchiveCallbackClient {
 public:
  explicit ArchiveTestClient() : file_count_(0), index_(0) {}
  // ArchiveCallbackClient methods
  virtual void ReceiveFileHeader(const ArchiveFileInfo &file_info);
  virtual bool ReceiveFileData(MemoryReadStream *stream, size_t nbytes);

  int GetFileCount() const { return file_count_; }
  size_t GetNumTotalBytesReceived() const { return index_; }

 private:
  int file_count_;
  int index_;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ArchiveTestClient::ReceiveFileHeader(const ArchiveFileInfo &file_info) {
  // We get called one time for each file in the archive

  // Check that the filenames match our expectation
  switch (file_count_) {
    case 0:
      EXPECT_TRUE(!strcmp(kFilename1, file_info.GetFileName().c_str()));
      break;
    case 1:
      EXPECT_TRUE(!strcmp(kFilename2, file_info.GetFileName().c_str()));
      break;
    case 2:
      EXPECT_TRUE(!strcmp(kFilename3, file_info.GetFileName().c_str()));
      break;
  }

  file_count_++;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool ArchiveTestClient::ReceiveFileData(MemoryReadStream *stream,
                                        size_t nbytes) {
  const char *p = reinterpret_cast<const char*>(
      stream->GetDirectMemoryPointer());

  // Note: ReceiveFileData() may be called multiple times for each file, until
  // the complete file contents have been given...

  // We're receiving three file, one after the other
  // The bytes we receive are the concatenated contents of the three files
  // (with calls to ReceiveFileHeader() separating each one)
  //
  EXPECT_TRUE(index_ + nbytes <= strlen(kConcatenatedContents));
  EXPECT_TRUE(!strncmp(kConcatenatedContents + index_, p, nbytes));

  index_ += nbytes;

  return true;
}

// Loads a tar.gz file, runs it through the processor.
// In our callbacks, we verify that we receive three files with known contents
//
TEST_F(TarGzProcessorTest, LoadTarGzFile) {
  String filepath = *g_program_path + "/archive_files/test1.tar.gz";

  ArchiveTestClient test_callback_client;

  TarGzProcessor processor(&test_callback_client);
  int result = processor.ProcessFile(filepath.c_str());
  EXPECT_EQ(Z_OK, result);

  EXPECT_EQ(3, test_callback_client.GetFileCount());
  EXPECT_EQ(strlen(kConcatenatedContents),
            test_callback_client.GetNumTotalBytesReceived());
}

// Tries to load something with a tar.gz extension, but which isn't
// really a tar.gz and contains random text
//
TEST_F(TarGzProcessorTest, LoadBogusTarGzFile) {
  String filepath = *g_program_path + "/archive_files/bogus.tar.gz";

  ArchiveTestClient test_callback_client;

  TarGzProcessor processor(&test_callback_client);
  int result = processor.ProcessFile(filepath.c_str());
  EXPECT_TRUE(result != Z_OK);
}

}  // namespace
