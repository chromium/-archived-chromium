// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/third_party/bsdiff.h"

#include <string>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"

#include "courgette/courgette.h"
#include "courgette/streams.h"

#include "testing/gtest/include/gtest/gtest.h"

class BSDiffMemoryTest : public testing::Test {
 public:
  std::string FileContents(const char *file_name) const;
  void GenerateAndTestPatch(const std::string& a, const std::string& b) const;

 private:
  void SetUp() {
    PathService::Get(base::DIR_SOURCE_ROOT, &test_dir_);
    file_util::AppendToPath(&test_dir_, L"courgette");
    file_util::AppendToPath(&test_dir_, L"testdata");
  }

  std::wstring test_dir_;
};

//  Reads a test file into a string.
std::string BSDiffMemoryTest::FileContents(const char *file_name) const {
  std::wstring file_path = test_dir_;
  file_util::AppendToPath(&file_path, UTF8ToWide(file_name));
  std::string file_bytes;
  if (!file_util::ReadFileToString(file_path, &file_bytes)) {
    EXPECT_TRUE(!"Could not read test data");
  }
  return file_bytes;
}

void BSDiffMemoryTest::GenerateAndTestPatch(const std::string& old_text,
                                            const std::string& new_text) const {
  courgette::SourceStream old1;
  courgette::SourceStream new1;
  old1.Init(old_text.c_str(), old_text.length());
  new1.Init(new_text.c_str(), new_text.length());

  courgette::SinkStream patch1;
  courgette::BSDiffStatus status = CreateBinaryPatch(&old1, &new1, &patch1);
  EXPECT_EQ(courgette::OK, status);

  courgette::SourceStream old2;
  courgette::SourceStream patch2;
  old2.Init(old_text.c_str(), old_text.length());
  patch2.Init(patch1);

  courgette::SinkStream new2;
  status = ApplyBinaryPatch(&old2, &patch2, &new2);
  EXPECT_EQ(courgette::OK, status);
  EXPECT_EQ(new_text.length(), new2.Length());
  EXPECT_EQ(0, memcmp(new_text.c_str(), new2.Buffer(), new_text.length()));
}

TEST_F(BSDiffMemoryTest, TestEmpty) {
  GenerateAndTestPatch("", "");
}

TEST_F(BSDiffMemoryTest, TestEmptyVsNonempty) {
  GenerateAndTestPatch("", "xxx");
}

TEST_F(BSDiffMemoryTest, TestNonemptyVsEmpty) {
  GenerateAndTestPatch("xxx", "");
}

TEST_F(BSDiffMemoryTest, TestSmallInputsWithSmallChanges) {
  std::string file1 =
      "I would not, could not, in a box.\n"
      "I could not, would not, with a fox.\n"
      "I will not eat them with a mouse.\n"
      "I will not eat them in a house.\n"
      "I will not eat them here or there.\n"
      "I will not eat them anywhere.\n"
      "I do not eat green eggs and ham.\n"
      "I do not like them, Sam-I-am.\n";
  std::string file2 =
      "I would not, could not, in a BOX.\n"
      "I could not, would not, with a FOX.\n"
      "I will not eat them with a MOUSE.\n"
      "I will not eat them in a HOUSE.\n"
      "I will not eat them in a HOUSE.\n"     // Extra line.
      "I will not eat them here or THERE.\n"
      "I will not eat them ANYWHERE.\n"
      "I do not eat green eggs and HAM.\n"
      "I do not like them, Sam-I-am.\n";
  GenerateAndTestPatch(file1, file2);
}

TEST_F(BSDiffMemoryTest, TestIndenticalDlls) {
  std::string file1 = FileContents("en-US.dll");
  GenerateAndTestPatch(file1, file1);
}

TEST_F(BSDiffMemoryTest, TestDifferentExes) {
  std::string file1 = FileContents("setup1.exe");
  std::string file2 = FileContents("setup2.exe");
  GenerateAndTestPatch(file1, file2);
}
