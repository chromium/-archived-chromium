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


// This file contains the tests of class StringReader.
#include <stdio.h>

#include "tests/common/win/testing_common.h"
#include "utils/cross/string_reader.h"

namespace o3d {

namespace {
static const char* kTestStrings[] = {
  "testing 1..2..3",
  "4..5..6",
  "testing 1..2..3\n4..5..6\n",
  "testing 1..2..3\r4..5..6\r",
  "testing 1..2..3\r\n4..5..6\r\n",
  "testing 1..2..3\n\r4..5..6\n\r",
  "testing 1..2..3\n\n4..5..6\n\n",
};
}  // end anonymous namespace

class StringReaderTest : public testing::Test {
 public:
  StringReaderTest() {
    test_string_one_ = kTestStrings[0];
    test_string_two_ = kTestStrings[1];
    test_string_lf_ = kTestStrings[2];
    test_string_cr_ = kTestStrings[3];
    test_string_crlf_ = kTestStrings[4];
    test_string_lfcr_ = kTestStrings[5];
    test_string_lflf_ = kTestStrings[6];
  }

  std::string test_string_one_;
  std::string test_string_two_;
  std::string test_string_lf_;
  std::string test_string_cr_;
  std::string test_string_crlf_;
  std::string test_string_lfcr_;
  std::string test_string_lflf_;
};

TEST_F(StringReaderTest, StartAtBeginning) {
  StringReader reader(test_string_one_);
  EXPECT_EQ(0, reader.position());
  EXPECT_EQ(test_string_one_, reader.input());
}

TEST_F(StringReaderTest, TestPeekString) {
  StringReader reader(test_string_one_);
  EXPECT_EQ(test_string_one_.substr(0, 6), reader.PeekString(6));
  EXPECT_EQ(0, reader.position());
  EXPECT_EQ(test_string_one_, reader.input());
}

TEST_F(StringReaderTest, ReadsSingleCharacter) {
  StringReader reader(test_string_one_);
  EXPECT_EQ(test_string_one_.substr(0, 1)[0], reader.ReadChar());
  EXPECT_EQ(1, reader.position());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ(test_string_one_.substr(1, 2), reader.PeekString(2));
  EXPECT_EQ(1, reader.position());
}

TEST_F(StringReaderTest, ReadsMultipleCharacters) {
  StringReader reader(test_string_one_);
  EXPECT_EQ(test_string_one_.substr(0, 1)[0], reader.ReadChar());
  EXPECT_EQ(test_string_one_.substr(1, 1)[0], reader.ReadChar());
  EXPECT_EQ(2, reader.position());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ(test_string_one_.substr(2, 2), reader.PeekString(2));
  EXPECT_EQ(2, reader.position());
}

TEST_F(StringReaderTest, ReadsString) {
  StringReader reader(test_string_one_);
  EXPECT_EQ(test_string_one_.substr(0, 7), reader.ReadString(7));
  EXPECT_EQ(7, reader.position());
  EXPECT_EQ(false, reader.IsAtEnd());
  EXPECT_EQ(test_string_one_.substr(7, 2), reader.PeekString(2));
  EXPECT_EQ(7, reader.position());
}

TEST_F(StringReaderTest, ReadsToEnd) {
  StringReader reader(test_string_lf_);
  EXPECT_EQ(test_string_lf_, reader.ReadToEnd());
  EXPECT_EQ(test_string_lf_.size(), reader.position());
  EXPECT_EQ(true, reader.IsAtEnd());
  EXPECT_EQ("", reader.ReadString(1));
  EXPECT_EQ(true, reader.IsAtEnd());
  EXPECT_EQ(test_string_lf_.size(), reader.position());
}


TEST_F(StringReaderTest, ReadsLinefeedString) {
  StringReader reader(test_string_lf_);
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

TEST_F(StringReaderTest, ReadsCarriageReturnString) {
  StringReader reader(test_string_cr_);
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


TEST_F(StringReaderTest, ReadsCarriageReturnLinefeedString) {
  StringReader reader(test_string_crlf_);
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

TEST_F(StringReaderTest, ReadsLinefeedCarriageReturnString) {
  StringReader reader(test_string_lfcr_);
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

TEST_F(StringReaderTest, ReadsLinefeedLinefeedString) {
  StringReader reader(test_string_lflf_);
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
