// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "testing/gtest/include/gtest/gtest.h"
#include "base/json_writer.h"
#include "base/values.h"

TEST(JSONWriterTest, Writing) {
  // Test null
  Value* root = Value::CreateNullValue();
  std::string output_js;
  JSONWriter::Write(root, false, &output_js);
  ASSERT_EQ("null", output_js);
  delete root;

  // Test empty dict
  root = new DictionaryValue;
  JSONWriter::Write(root, false, &output_js);
  ASSERT_EQ("{}", output_js);
  delete root;

  // Test empty list
  root = new ListValue;
  JSONWriter::Write(root, false, &output_js);
  ASSERT_EQ("[]", output_js);
  delete root;

  // Test Real values should always have a decimal or an 'e'.
  root = Value::CreateRealValue(1.0);
  JSONWriter::Write(root, false, &output_js);
  ASSERT_EQ("1.0", output_js);
  delete root;

  // Writer unittests like empty list/dict nesting,
  // list list nesting, etc.
  DictionaryValue root_dict;
  ListValue* list = new ListValue;
  root_dict.Set(L"list", list);
  DictionaryValue* inner_dict = new DictionaryValue;
  list->Append(inner_dict);
  inner_dict->SetInteger(L"inner int", 10);
  ListValue* inner_list = new ListValue;
  list->Append(inner_list);
  list->Append(Value::CreateBooleanValue(true));

  JSONWriter::Write(&root_dict, false, &output_js);
  ASSERT_EQ("{\"list\":[{\"inner int\":10},[],true]}", output_js);
  JSONWriter::Write(&root_dict, true, &output_js);
  ASSERT_EQ("{\r\n"
            "   \"list\": [ {\r\n"
            "      \"inner int\": 10\r\n"
            "   }, [  ], true ]\r\n"
            "}\r\n",
            output_js);
}

