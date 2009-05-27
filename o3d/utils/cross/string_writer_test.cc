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


// This file contains the tests of class StringWriter.

#include "tests/common/win/testing_common.h"
#include "utils/cross/string_writer.h"

namespace o3d {

class StringWriterTest : public testing::Test {
};

TEST_F(StringWriterTest, EmptyByDefault) {
  StringWriter writer(StringWriter::CR);
  EXPECT_EQ(String(), writer.ToString());
}

TEST_F(StringWriterTest, WritesSingleCharacter) {
  StringWriter writer(StringWriter::CR);
  writer.WriteChar('y');
  EXPECT_EQ(String("y"), writer.ToString());
}

TEST_F(StringWriterTest, WritesMultipleCharacters) {
  StringWriter writer(StringWriter::CR);
  writer.WriteChar('y');
  writer.WriteChar('z');
  EXPECT_EQ(String("yz"), writer.ToString());
}

TEST_F(StringWriterTest, WritesString) {
  StringWriter writer(StringWriter::CR);
  writer.WriteString(String("hello"));
  EXPECT_EQ(String("hello"), writer.ToString());
}

TEST_F(StringWriterTest, WritesCarriageReturnNewLine) {
  StringWriter writer(StringWriter::CR);
  writer.WriteNewLine();
  EXPECT_EQ(String("\r"), writer.ToString());
}

TEST_F(StringWriterTest, WritesCarriageReturnLineFeedNewLine) {
  StringWriter writer(StringWriter::CR_LF);
  writer.WriteNewLine();
  EXPECT_EQ(String("\r\n"), writer.ToString());
}

TEST_F(StringWriterTest, WritesLineFeedNewLine) {
  StringWriter writer(StringWriter::LF);
  writer.WriteNewLine();
  EXPECT_EQ(String("\n"), writer.ToString());
}

TEST_F(StringWriterTest, WritesFalse) {
  StringWriter writer(StringWriter::LF);
  writer.WriteBool(false);
  EXPECT_EQ(String("false"), writer.ToString());
}

TEST_F(StringWriterTest, WritesTrue) {
  StringWriter writer(StringWriter::LF);
  writer.WriteBool(true);
  EXPECT_EQ(String("true"), writer.ToString());
}

TEST_F(StringWriterTest, WritesInt) {
  StringWriter writer(StringWriter::LF);
  writer.WriteInt(-123);
  EXPECT_EQ(String("-123"), writer.ToString());
}

TEST_F(StringWriterTest, WritesUnsignedInt) {
  StringWriter writer(StringWriter::LF);
  writer.WriteInt(123u);
  ASSERT_EQ(String("123"), writer.ToString());
}

TEST_F(StringWriterTest, WritesFloat) {
  StringWriter writer(StringWriter::LF);
  writer.WriteFloat(1.25f);
  EXPECT_EQ(String("1.25"), writer.ToString());
}

TEST_F(StringWriterTest, WritesPrintfFormatted) {
  StringWriter writer(StringWriter::LF);
  writer.WriteFormatted("%s: %d", "age", 31);
  EXPECT_EQ(String("age: 31"), writer.ToString());
}
}  // namespace o3d
