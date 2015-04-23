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
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"
#include "import/cross/tar_generator.h"
#include "tests/common/win/testing_common.h"
#include "tests/common/cross/test_utils.h"

namespace o3d {

using o3d::StreamProcessor;
using o3d::MemoryReadStream;
using o3d::MemoryWriteStream;
using o3d::TarGenerator;

class TarGeneratorTest : public testing::Test {
};

const int kBlockSize              = 512;

const int kMaxFilenameSize        = 100;

const int kFileNameOffset         = 0;
const int kFileModeOffset         = 100;
const int kUserIDOffset           = 108;
const int kGroupIDOffset          = 116;
const int kFileSizeOffset         = 124;
const int kModifyTimeOffset       = 136;
const int kHeaderCheckSumOffset   = 148;
const int kLinkFlagOffset         = 156;
const int kMagicOffset            = 257;
const int kUserNameOffset         = 265;
const int kGroupNameOffset        = 297;

const char *kDirName1 = "test/apples/";
const char *kDirName2 = "test/oranges/";
const char *kFileName1 = "test/apples/file1";
const char *kFileName2 = "test/apples/file2";
const char *kFileName3 = "test/oranges/file3";

// The first file is less than one block in size
const char *kFileContents1 =
    "The cellphone is the world’s most ubiquitous computer.\n"
    "The four billion cellphones in use around the globe carry personal\n"
    "information, provide access to the Web and are being used more and more\n"
    "to navigate the real world. And as cellphones change how we live,\n"
    "computer scientists say, they are also changing\n"
    "how we think about information\n";

// The 2nd file takes two blocks
const char *kFileContents2 =
    "From Hong Kong to eastern Europe to Wall Street, financial gloom was\n"
    "everywhere on Tuesday.\n"
    "Stock markets around the world staggered lower. In New York,\n"
    "the Dow fell more than 3 percent, coming within sight of its worst\n"
    "levels since the credit crisis erupted. Financial shares were battered.\n"
    "And rattled investors clamored to buy rainy-day investments like gold\n"
    "and Treasury debt. It was a global wave of selling spurred by rising\n"
    "worries about how banks, automakers — entire countries — would fare\n"
    "in a deepening global downturn.\n"
    "'Nobody believes it’s going get better yet,' said Howard Silverblatt,\n"
    "senior index analyst at Standard & Poor’s. 'Do you see that light at\n"
    "the end of the tunnel? Any kind of light? Right now, it’s not there'\n"
    "yet.\n";

// The 3rd file takes one block
const char *kFileContents3 = "nothing much here...\n";

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Receives the tar bytestream from the TarGenerator.
// We validate the bytestream as it comes in...
//
class CallbackClient : public StreamProcessor {
 public:
  // states for state machine, with each state representing one
  // received block of the tar stream.
  // The blocks can be either headers (for directories and files)
  // or data blocks (zero padded at the end to make a full block)
  enum ValidationState {
    VALIDATE_DIRECTORY_HEADER1,  // header 1 is directory so no file data
    VALIDATE_FILE_HEADER1,
    VALIDATE_FILE_DATA1,         // 1st file takes one block
    VALIDATE_FILE_HEADER2,
    VALIDATE_FILE_DATA2_BLOCK1,  // 2nd file takes two blocks
    VALIDATE_FILE_DATA2_BLOCK2,
    VALIDATE_DIRECTORY_HEADER2,  // 3rd file is in another directory
    VALIDATE_FILE_HEADER3,
    VALIDATE_FILE_DATA3,
    FINISHED
  };

  CallbackClient()
      : state_(VALIDATE_DIRECTORY_HEADER1),
        total_bytes_received_(0),
        memory_block_(kBlockSize),
        write_stream_(memory_block_, kBlockSize) {
  }

  virtual int     ProcessBytes(MemoryReadStream *stream,
                               size_t bytes_to_process);

  size_t          GetTotalBytesReceived() { return total_bytes_received_; }
  int             GetState() { return state_; }

 private:
  bool            IsOctalDigit(uint8 c);
  bool            IsOctalString(uint8 *p);
  unsigned int    ComputeCheckSum(uint8 *header);

  void            ValidateData(uint8 *header, const char *file_contents);

  void            ValidateHeader(uint8 *header,
                                 const char *file_name,
                                 size_t file_length);

  // For debugging
  void            DumpMemoryBlock(uint8 *block);

  int                 state_;
  size_t              total_bytes_received_;
  MemoryBuffer<uint8> memory_block_;
  MemoryWriteStream   write_stream_;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int CallbackClient::ProcessBytes(MemoryReadStream *stream,
                                 size_t bytes_to_process) {
  total_bytes_received_ += bytes_to_process;

  while (bytes_to_process > 0) {
    size_t remaining = write_stream_.GetRemainingByteCount();

    size_t bytes_this_time =
        bytes_to_process < remaining ? bytes_to_process : remaining;

    const uint8 *p = stream->GetDirectMemoryPointer();
    stream->Skip(bytes_this_time);

    write_stream_.Write(p, bytes_this_time);

    // our block buffer is full, so validate it according to our state
    // machine
    if (write_stream_.GetRemainingByteCount() == 0) {
      // DumpMemoryBlock(memory_block_);  // uncomment for debugging

      switch (state_) {
        case VALIDATE_DIRECTORY_HEADER1:
          ValidateHeader(memory_block_, kDirName1, 0);
          break;

        case VALIDATE_FILE_HEADER1:
          ValidateHeader(memory_block_, kFileName1, strlen(kFileContents1));
          break;

        case VALIDATE_FILE_DATA1:
          ValidateData(memory_block_, kFileContents1);
          break;

        case VALIDATE_FILE_HEADER2:
          ValidateHeader(memory_block_, kFileName2, strlen(kFileContents2));
          break;

        case VALIDATE_FILE_DATA2_BLOCK1:
          // file2 data is larger than one one block, so we'll verify
          // the two blocks are correct
          ValidateData(memory_block_, kFileContents2);
          break;

        case VALIDATE_FILE_DATA2_BLOCK2:
          ValidateData(memory_block_, kFileContents2 + kBlockSize);
          break;

        case VALIDATE_DIRECTORY_HEADER2:
          ValidateHeader(memory_block_, kDirName2, 0);
          break;

        case VALIDATE_FILE_HEADER3:
          ValidateHeader(memory_block_, kFileName3, strlen(kFileContents3));
          break;

        case VALIDATE_FILE_DATA3:
          ValidateData(memory_block_, kFileContents3);
          break;

        case FINISHED:
          break;
      }

      // Advance to the next state
      ++state_;

      // So next time we write, we start at beginning of buffer
      write_stream_.Seek(0);
    }

    bytes_to_process -= bytes_this_time;
  }

  return 0;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool CallbackClient::IsOctalDigit(uint8 c) {
  return (c >= '0' && c <= '7');
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool CallbackClient::IsOctalString(uint8 *p) {
  while (IsOctalDigit(*p)) ++p;
  return *p == 0;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
unsigned int CallbackClient::ComputeCheckSum(uint8 *header) {
  unsigned int checksum = 0;
  for (int i = 0; i < kBlockSize; ++i) {
    uint8 value = header[i];
    if (i >= kHeaderCheckSumOffset && i < kHeaderCheckSumOffset + 8) {
      value = 32;  // treat checksum itself as ' '
    }
    checksum += value;
  }
  return checksum;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// for now |file_contents| must be less than a block size...
void CallbackClient::ValidateData(uint8 *header,
                                  const char *file_contents) {
  // only check up to one block size worth of data
  int zero_pad_start_index = strlen(file_contents);
  if (zero_pad_start_index > kBlockSize) zero_pad_start_index = kBlockSize;

  if (zero_pad_start_index <= kBlockSize) {
    // file data must match
    uint8 *p = memory_block_;
    EXPECT_EQ(0, strncmp(file_contents, (const char*)p, kBlockSize));

    // check that zero padding is there
    bool zero_padding_good = true;
    for (int i = zero_pad_start_index; i < kBlockSize; ++i) {
      if (memory_block_[i] != 0) {
        zero_padding_good = false;
        break;
      }
    }
    EXPECT_TRUE(zero_padding_good);
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void CallbackClient::ValidateHeader(uint8 *header,
                                    const char *file_name,
                                    size_t file_length) {
  // Validate file name
  EXPECT_EQ(0,
            strcmp(reinterpret_cast<char*>(header) + kFileNameOffset,
                   file_name));

  // Validate length
  int length_in_header;
  sscanf((const char*)header + kFileSizeOffset, "%o", &length_in_header);
  EXPECT_EQ(file_length, length_in_header);


  EXPECT_EQ(0, header[kMaxFilenameSize - 1]);

  EXPECT_TRUE(IsOctalString(header + kFileModeOffset));
  EXPECT_EQ(0, header[kFileModeOffset + 7]);

  EXPECT_TRUE(IsOctalString(header + kUserIDOffset));
  EXPECT_EQ(0, header[kUserIDOffset + 7]);

  EXPECT_TRUE(IsOctalString(header + kGroupIDOffset));
  EXPECT_EQ(0, header[kGroupIDOffset + 7]);

  EXPECT_TRUE(IsOctalString(header + kFileSizeOffset));
  EXPECT_EQ(0, header[kFileSizeOffset + 11]);

  EXPECT_TRUE(IsOctalString(header + kModifyTimeOffset));
  EXPECT_EQ(0, header[kModifyTimeOffset + 11]);

  EXPECT_TRUE(IsOctalString(header + kHeaderCheckSumOffset));
  EXPECT_EQ(0, header[kHeaderCheckSumOffset + 6]);

  // For now we only have directories '5' or normal files '0'
  int link_flag = header[kLinkFlagOffset];
  EXPECT_TRUE(link_flag == '0' || link_flag == '5');

  EXPECT_EQ(0, strcmp((const char*)header + kMagicOffset, "ustar  "));

  EXPECT_EQ(0, header[kUserNameOffset + 31]);
  EXPECT_EQ(0, header[kGroupNameOffset + 31]);

  // Validate checksum
  int checksum = ComputeCheckSum(header);
  int header_checksum;
  sscanf((const char*)header + kHeaderCheckSumOffset, "%o", &header_checksum);

  EXPECT_EQ(checksum, header_checksum);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// For debugging purposes
void CallbackClient::DumpMemoryBlock(uint8 *block) {
  for (int i = 0; i < kBlockSize; ++i) {
    if ((i % 16) == 0) printf("\n");
    char c = block[i];
    if (c == 0) {
      c = '.';
    }
    printf("%c", c);
  }
  printf("\n");
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Creates a tar file with three files in two directories
//
TEST_F(TarGeneratorTest, CreateSimpleArchive) {
  CallbackClient client;
  TarGenerator generator(&client);

  const int kFileLength1 = strlen(kFileContents1);
  const int kFileLength2 = strlen(kFileContents2);
  const int kFileLength3 = strlen(kFileContents3);

  generator.AddFile(kFileName1, kFileLength1);
  MemoryReadStream file1_stream(reinterpret_cast<const uint8*>(kFileContents1),
                                kFileLength1);
  generator.AddFileBytes(&file1_stream, kFileLength1);

  generator.AddFile(kFileName2, kFileLength2);
  MemoryReadStream file2_stream(reinterpret_cast<const uint8*>(kFileContents2),
                                kFileLength2);
  generator.AddFileBytes(&file2_stream, kFileLength2);

  generator.AddFile(kFileName3, kFileLength3);
  MemoryReadStream file3_stream(reinterpret_cast<const uint8*>(kFileContents3),
                                kFileLength3);
  generator.AddFileBytes(&file3_stream, kFileLength3);

  generator.Finalize();

  // Verify that the tar byte stream produced is exactly divisible by
  // the block size
  size_t bytes_received = client.GetTotalBytesReceived();
  EXPECT_EQ(0, bytes_received % kBlockSize);

  // Make sure the state machine is in the expected state
  EXPECT_EQ(CallbackClient::FINISHED, client.GetState());
}

}  // namespace
