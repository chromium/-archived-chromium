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


// This file contains the tests of class FileTextReader.
#include <stdio.h>

#include "base/basictypes.h"
#include "tests/common/win/testing_common.h"
#include "utils/cross/file_text_reader.h"

namespace o3d {

namespace {
struct FileInfo {
  const char* file_name_;
  const char* file_contents_;
};

static const FileInfo kFileInfo[] = {
  "/test_file_string_one", "testing 1..2..3",
  "/test_file_string_two", "4..5..6",
  "/test_file_string_lf", "testing 1..2..3\n4..5..6\n",
  "/test_file_string_cr", "testing 1..2..3\r4..5..6\r",
  "/test_file_string_crlf", "testing 1..2..3\r\n4..5..6\r\n",
  "/test_file_string_lfcr", "testing 1..2..3\n\r4..5..6\n\r",
  "/test_file_string_lflf", "testing 1..2..3\n\n4..5..6\n\n",
  "/test_file_string_short", "T",
  "/test_file_string_empty", "",
};

static const int kNumFiles = arraysize(kFileInfo);
}  // end anonymous namespace

class FileTextReaderTest : public testing::Test {
 public:
  FileTextReaderTest() {
    test_string_one_ = kFileInfo[0].file_contents_;
    test_string_two_ = kFileInfo[1].file_contents_;
    test_string_lf_ = kFileInfo[2].file_contents_;
    test_string_cr_ = kFileInfo[3].file_contents_;
    test_string_crlf_ = kFileInfo[4].file_contents_;
    test_string_lfcr_ = kFileInfo[5].file_contents_;
    test_string_lflf_ = kFileInfo[6].file_contents_;
    test_string_short_ = kFileInfo[7].file_contents_;
    test_string_empty_ = kFileInfo[8].file_contents_;
  }
  virtual void SetUp() {
    // This clears out the existing Temporary files and rewrites them.
    std::string tmp_base(g_program_path->data());
    for (int i = 0; i < kNumFiles; i++) {
      std::string filename = tmp_base + kFileInfo[i].file_name_;
      FILE* file_pointer = fopen(filename.c_str(), "wb+");
      int contents_size = ::strlen(kFileInfo[i].file_contents_);
      if (contents_size > 0) {
        ::fwrite(kFileInfo[i].file_contents_, sizeof(char), contents_size,
                 file_pointer);
        ::fseek(file_pointer, 0, SEEK_SET);
      }
      file_pointers_[i] = file_pointer;
    }
  }
  virtual void TearDown() {
    std::string tmp_base(g_program_path->data());
    // Clear out the tmp files.
    for (int i = 0; i < kNumFiles; i++) {
      if (file_pointers_[i]) {
        ::fclose(file_pointers_[i]);
      }
      std::string path = tmp_base + kFileInfo[i].file_name_;
#if defined(OS_WIN)
      ::_unlink(path.c_str());
#else
      ::unlink(path.c_str());
#endif
      file_pointers_[i] = NULL;
    }
  }

  FILE* file_pointers_[kNumFiles];
  std::string test_string_one_;
  std::string test_string_two_;
  std::string test_string_lf_;
  std::string test_string_cr_;
  std::string test_string_crlf_;
  std::string test_string_lfcr_;
  std::string test_string_lflf_;
  std::string test_string_short_;
  std::string test_string_empty_;
};

TEST_F(FileTextReaderTest, StartAtBeginning) {
  FileTextReader reader(file_pointers_[0]);
  EXPECT_EQ(0, reader.position());
  EXPECT_EQ(file_pointers_[0], reader.input());
  EXPECT_EQ(false, reader.IsAtEnd());
}

TEST_F(FileTextReaderTest, TestPeekString) {
  FileTextReader reader(file_pointers_[0]);
  EXPECT_EQ(test_string_one_.substr(0, 6), reader.PeekString(6));
  EXPECT_EQ(0, reader.position());
  EXPECT_EQ(false, reader.IsAtEnd());
}

TEST_F(FileTextReaderTest, ReadsSingleCharacter) {
  FileTextReader reader(file_pointers_[0]);
  EXPECT_EQ(test_string_one_.substr(0, 1)[0], reader.ReadChar());
  EXPECT_EQ(1, reader.position());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ(test_string_one_.substr(1, 2), reader.PeekString(2));
  EXPECT_EQ(1, reader.position());
}

TEST_F(FileTextReaderTest, ReadsMultipleCharacters) {
  FileTextReader reader(file_pointers_[0]);
  EXPECT_EQ(test_string_one_.substr(0, 1)[0], reader.ReadChar());
  EXPECT_EQ(test_string_one_.substr(1, 1)[0], reader.ReadChar());
  EXPECT_EQ(2, reader.position());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ(test_string_one_.substr(2, 2), reader.PeekString(2));
  EXPECT_EQ(2, reader.position());
}

TEST_F(FileTextReaderTest, ReadsFile) {
  FileTextReader reader(file_pointers_[0]);
  EXPECT_EQ(test_string_one_.substr(0, 7), reader.ReadString(7));
  EXPECT_EQ(7, reader.position());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ(test_string_one_.substr(7, 2), reader.PeekString(2));
  EXPECT_EQ(7, reader.position());
}

TEST_F(FileTextReaderTest, EmptyFile) {
  FileTextReader reader(file_pointers_[8]);
  EXPECT_EQ("", reader.PeekString(1));
  EXPECT_EQ(0, reader.position());
  EXPECT_EQ(true, reader.IsAtEnd());
  EXPECT_EQ(0, reader.ReadChar());
  EXPECT_EQ(true, reader.IsAtEnd());
}

TEST_F(FileTextReaderTest, TinyFile) {
  FileTextReader reader(file_pointers_[7]);
  EXPECT_EQ(test_string_short_.substr(0, 1), reader.PeekString(1));
  EXPECT_EQ(0, reader.position());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ(test_string_short_[0], reader.ReadChar());
  EXPECT_EQ(true, reader.IsAtEnd());
}

TEST_F(FileTextReaderTest, ReadsToEnd) {
  FileTextReader reader(file_pointers_[2]);
  EXPECT_EQ(test_string_lf_, reader.ReadToEnd());
  EXPECT_EQ(test_string_lf_.size(), reader.position());
  EXPECT_EQ(true, reader.IsAtEnd());
  EXPECT_EQ("", reader.ReadString(1));
  EXPECT_EQ(true, reader.IsAtEnd());
  EXPECT_EQ(test_string_lf_.size(), reader.position());
}


TEST_F(FileTextReaderTest, ReadsLinefeedFile) {
  FileTextReader reader(file_pointers_[2]);
  std::string line = reader.ReadLine();
  EXPECT_EQ(test_string_one_, line);
  EXPECT_EQ(test_string_one_.size() + 1, reader.position());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ(test_string_lf_.substr(test_string_one_.size() + 1, 2),
            reader.PeekString(2));
  EXPECT_EQ(test_string_one_.size() + 1, reader.position());
  EXPECT_EQ(test_string_two_, reader.ReadLine());
  EXPECT_EQ(true, reader.IsAtEnd());
  EXPECT_EQ(test_string_lf_.size(), reader.position());
}

TEST_F(FileTextReaderTest, ReadsCarriageReturnFile) {
  FileTextReader reader(file_pointers_[3]);
  EXPECT_EQ(test_string_one_, reader.ReadLine());
  EXPECT_EQ(test_string_one_.size() + 1, reader.position());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ(test_string_cr_.substr(test_string_one_.size() + 1, 2),
            reader.PeekString(2));
  EXPECT_EQ(test_string_one_.size() + 1, reader.position());
  EXPECT_EQ(test_string_two_, reader.ReadLine());
  EXPECT_EQ(true, reader.IsAtEnd());
  EXPECT_EQ(test_string_cr_.size(), reader.position());
}


TEST_F(FileTextReaderTest, ReadsCarriageReturnLinefeedFile) {
  FileTextReader reader(file_pointers_[4]);
  EXPECT_EQ(test_string_one_, reader.ReadLine());
  EXPECT_EQ(test_string_one_.size() + 2, reader.position());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ(test_string_crlf_.substr(test_string_one_.size() + 2, 2),
            reader.PeekString(2));
  EXPECT_EQ(test_string_one_.size() + 2, reader.position());
  EXPECT_EQ(test_string_two_, reader.ReadLine());
  EXPECT_EQ(true, reader.IsAtEnd());
  EXPECT_EQ(test_string_crlf_.size(), reader.position());
}

TEST_F(FileTextReaderTest, ReadsLinefeedCarriageReturnFile) {
  FileTextReader reader(file_pointers_[5]);
  EXPECT_EQ(test_string_one_, reader.ReadLine());
  EXPECT_EQ(test_string_one_.size() + 1, reader.position());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ(test_string_lfcr_.substr(test_string_one_.size() + 1, 2),
            reader.PeekString(2));
  EXPECT_EQ(test_string_one_.size() + 1, reader.position());
  EXPECT_EQ("", reader.ReadLine());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ(test_string_two_, reader.ReadLine());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ("", reader.ReadLine());
  EXPECT_EQ(true, reader.IsAtEnd());
  EXPECT_EQ(test_string_lfcr_.size(), reader.position());
}

TEST_F(FileTextReaderTest, ReadsLinefeedLinefeedFile) {
  FileTextReader reader(file_pointers_[6]);
  EXPECT_EQ(test_string_one_, reader.ReadLine());
  EXPECT_EQ(test_string_one_.size() + 1, reader.position());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ(test_string_lflf_.substr(test_string_one_.size() + 1, 2),
            reader.PeekString(2));
  EXPECT_EQ(test_string_one_.size() + 1, reader.position());
  EXPECT_EQ("", reader.ReadLine());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ(test_string_two_, reader.ReadLine());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ("", reader.ReadLine());
  EXPECT_EQ(true, reader.IsAtEnd());
  EXPECT_EQ(test_string_lflf_.size(), reader.position());
}

}  // namespace o3d
