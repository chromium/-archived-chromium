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


// This file contains the tests of class JsonWriter.

#include "tests/common/win/testing_common.h"
#include "utils/cross/json_writer.h"
#include "utils/cross/string_writer.h"

namespace o3d {

class JsonWriterTest : public testing::Test {
 public:
  JsonWriterTest()
      : output_(StringWriter::CR_LF),
        json_writer_(&output_, 2) {
  }
 protected:
  StringWriter output_;
  JsonWriter json_writer_;
};

TEST_F(JsonWriterTest, WritesEmptyObject) {
  json_writer_.OpenObject();
  json_writer_.CloseObject();
  json_writer_.Close();
  ASSERT_EQ(String("{\r\n}\r\n"), output_.ToString());
}

TEST_F(JsonWriterTest, WritesNestedObjects) {
  json_writer_.OpenObject();
  json_writer_.OpenObject();
  json_writer_.CloseObject();
  json_writer_.OpenObject();
  json_writer_.CloseObject();
  json_writer_.CloseObject();
  json_writer_.Close();
  ASSERT_EQ(String("{\r\n  {\r\n  },\r\n  {\r\n  }\r\n}\r\n"),
            output_.ToString());
}

TEST_F(JsonWriterTest, WritesObjectProperty) {
  json_writer_.OpenObject();
  json_writer_.WritePropertyName(String("myProperty"));
  json_writer_.WriteFloat(1.25f);
  json_writer_.CloseObject();
  json_writer_.Close();
  ASSERT_EQ(String("{\r\n  \"myProperty\": 1.25\r\n}\r\n"),
            output_.ToString());
}

TEST_F(JsonWriterTest, EscapesSpecialCharsInObjectPropertyName) {
  json_writer_.OpenObject();
  json_writer_.WritePropertyName(String("\"\\\b\f\n\r\t\x01"));
  json_writer_.WriteFloat(1.25f);
  json_writer_.CloseObject();
  json_writer_.Close();
  ASSERT_EQ(
      String("{\r\n  \"\\\"\\\\\\b\\f\\n\\r\\t\\u0001\": 1.25\r\n}\r\n"),
      output_.ToString());
}
TEST_F(JsonWriterTest, WritesCommaSeparatedObjectProperties) {
  json_writer_.OpenObject();
  json_writer_.WritePropertyName(String("myProperty1"));
  json_writer_.WriteFloat(1.25f);
  json_writer_.WritePropertyName(String("myProperty2"));
  json_writer_.WriteFloat(2.5f);
  json_writer_.CloseObject();
  json_writer_.Close();
  ASSERT_EQ(
      String(
          "{\r\n  \"myProperty1\": 1.25,\r\n  \"myProperty2\": 2.5\r\n}\r\n"),
          output_.ToString());
}

TEST_F(JsonWriterTest, WritesEmptyArray) {
  json_writer_.OpenArray();
  json_writer_.CloseArray();
  json_writer_.Close();
  ASSERT_EQ(String("[\r\n]\r\n"), output_.ToString());
}

TEST_F(JsonWriterTest, WritesNestedArrays) {
  json_writer_.OpenArray();
  json_writer_.OpenArray();
  json_writer_.CloseArray();
  json_writer_.OpenArray();
  json_writer_.CloseArray();
  json_writer_.CloseArray();
  json_writer_.Close();
  ASSERT_EQ(String("[\r\n  [\r\n  ],\r\n  [\r\n  ]\r\n]\r\n"),
            output_.ToString());
}

TEST_F(JsonWriterTest, WritesArrayOfBools) {
  json_writer_.OpenArray();
  json_writer_.WriteBool(false);
  json_writer_.WriteBool(true);
  json_writer_.CloseArray();
  json_writer_.Close();
  ASSERT_EQ(String("[\r\n  false,\r\n  true\r\n]\r\n"),
            output_.ToString());
}

TEST_F(JsonWriterTest, WritesArrayOfInts) {
  json_writer_.OpenArray();
  json_writer_.WriteInt(1);
  json_writer_.WriteInt(2);
  json_writer_.CloseArray();
  json_writer_.Close();
  ASSERT_EQ(String("[\r\n  1,\r\n  2\r\n]\r\n"), output_.ToString());
}

TEST_F(JsonWriterTest, WritesArrayOfUnsignedInts) {
  json_writer_.OpenArray();
  json_writer_.WriteUnsignedInt(1u);
  json_writer_.WriteUnsignedInt(2u);
  json_writer_.CloseArray();
  json_writer_.Close();
  ASSERT_EQ(String("[\r\n  1,\r\n  2\r\n]\r\n"), output_.ToString());
}

TEST_F(JsonWriterTest, WritesArrayOfFloats) {
  json_writer_.OpenArray();
  json_writer_.WriteFloat(1);
  json_writer_.WriteFloat(2);
  json_writer_.CloseArray();
  json_writer_.Close();
  ASSERT_EQ(String("[\r\n  1,\r\n  2\r\n]\r\n"), output_.ToString());
}

TEST_F(JsonWriterTest, WritesArrayOfStrings) {
  json_writer_.OpenArray();
  json_writer_.WriteString(String("abc"));
  json_writer_.WriteString(String("def"));
  json_writer_.CloseArray();
  json_writer_.Close();
  ASSERT_EQ(String("[\r\n  \"abc\",\r\n  \"def\"\r\n]\r\n"),
            output_.ToString());
}

TEST_F(JsonWriterTest, EscapesSpecialCharsInString) {
  json_writer_.WriteString(String("\"\\\b\f\n\r\t\x01"));
  json_writer_.Close();
  ASSERT_EQ(String("\"\\\"\\\\\\b\\f\\n\\r\\t\\u0001\"\r\n"),
            output_.ToString());
}

TEST_F(JsonWriterTest, WritesArrayOfNulls) {
  json_writer_.OpenArray();
  json_writer_.WriteNull();
  json_writer_.WriteNull();
  json_writer_.CloseArray();
  json_writer_.Close();
  ASSERT_EQ(String("[\r\n  null,\r\n  null\r\n]\r\n"), output_.ToString());
}

TEST_F(JsonWriterTest, WritesWithoutWhiteSpaceIfCompacted) {
  json_writer_.BeginCompacting();
  json_writer_.OpenObject();
  json_writer_.WritePropertyName(String("foo"));
  json_writer_.WriteNull();
  json_writer_.WritePropertyName(String("bar"));
  json_writer_.WriteNull();
  json_writer_.CloseObject();
  json_writer_.EndCompacting();
  json_writer_.Close();
  ASSERT_EQ(String("{\"foo\":null,\"bar\":null}\r\n"), output_.ToString());
}

TEST_F(JsonWriterTest, ShouldWritePendingWhiteSpaceBeforeCompactedElements) {
  json_writer_.OpenArray();
  json_writer_.BeginCompacting();
  json_writer_.OpenObject();
  json_writer_.WritePropertyName(String("foo"));
  json_writer_.WriteNull();
  json_writer_.CloseObject();
  json_writer_.EndCompacting();
  json_writer_.CloseArray();
  json_writer_.Close();
  ASSERT_EQ(String("[\r\n  {\"foo\":null}\r\n]\r\n"), output_.ToString());
}
}  // namespace o3d
