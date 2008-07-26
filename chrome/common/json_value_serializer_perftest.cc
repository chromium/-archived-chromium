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

#include <vector>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/perftimer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/logging_chrome.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class JSONValueSerializerTests : public testing::Test {
 protected:
  virtual void SetUp() {
    static const wchar_t* const kTestFilenames[] = {
      L"serializer_nested_test.js",
      L"serializer_test.js",
      L"serializer_test_nowhitespace.js",
    };

    // Load test cases
    for (size_t i = 0; i < arraysize(kTestFilenames); ++i) {
      std::wstring filename;
      EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &filename));
      file_util::AppendToPath(&filename, kTestFilenames[i]);

      std::string test_case;
      EXPECT_TRUE(file_util::ReadFileToString(filename, &test_case));
      test_cases_.push_back(test_case);
    }
  }

  // Holds json strings to be tested.
  std::vector<std::string> test_cases_;
};

}  // namespace

// Test deserialization of a json string into a Value object.  We run the test
// using 3 sample strings for both the current decoder and jsoncpp's decoder.
TEST_F(JSONValueSerializerTests, Reading) {
  printf("\n");
  const int kIterations = 100000;

  // Test chrome json implementation
  PerfTimeLogger chrome_timer("chrome");
  for (int i = 0; i < kIterations; ++i) {
    for (size_t j = 0; j < test_cases_.size(); ++j) {
      Value* root = NULL;
      JSONStringValueSerializer reader(test_cases_[j]);
      ASSERT_TRUE(reader.Deserialize(&root));
      delete root;
    }
  }
  chrome_timer.Done();
}

TEST_F(JSONValueSerializerTests, CompactWriting) {
  printf("\n");
  const int kIterations = 100000;
  // Convert test cases to Value objects.
  std::vector<Value*> test_cases;
  for (size_t i = 0; i < test_cases_.size(); ++i) {
    Value* root = NULL;
    JSONStringValueSerializer reader(test_cases_[i]);
    ASSERT_TRUE(reader.Deserialize(&root));
    test_cases.push_back(root);
  }

  PerfTimeLogger chrome_timer("chrome");
  for (int i = 0; i < kIterations; ++i) {
    for (size_t j = 0; j < test_cases.size(); ++j) {
      std::string json;
      JSONStringValueSerializer reader(&json);
      ASSERT_TRUE(reader.Serialize(*test_cases[j]));
    }
  }
  chrome_timer.Done();

  // Clean up test cases.
  for (size_t i = 0; i < test_cases.size(); ++i) {
    delete test_cases[i];
    test_cases[i] = NULL;
  }
}
