// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/image_info.h"

#include <string>

#include "base/path_service.h"
#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class ImageInfoTest : public testing::Test {
 public:

  void TestExe() const;
  void TestResourceDll() const;

 private:
  void SetUp() {
    PathService::Get(base::DIR_SOURCE_ROOT, &test_dir_);
    file_util::AppendToPath(&test_dir_, L"courgette");
    file_util::AppendToPath(&test_dir_, L"testdata");
  }

  void TearDown() {
  }

  void ExpectExecutable(courgette::PEInfo* info) const;

  std::string FileContents(const char* file_name) const;

  std::wstring test_dir_;
};

//  Reads a test file into a string.
std::string ImageInfoTest::FileContents(const char* file_name) const {
  std::wstring file_path = test_dir_;
  file_util::AppendToPath(&file_path, UTF8ToWide(file_name));
  std::string file_bytes;
  if (!file_util::ReadFileToString(file_path, &file_bytes)) {
    EXPECT_TRUE(!"Could not read test data");
  }
  return file_bytes;
}

void ImageInfoTest::ExpectExecutable(courgette::PEInfo* info) const {
  EXPECT_TRUE(info->ok());
  EXPECT_TRUE(info->has_text_section());
}

void ImageInfoTest::TestExe() const {
  std::string file1 = FileContents("setup1.exe");

  scoped_ptr<courgette::PEInfo> info(new courgette::PEInfo());
  info->Init(reinterpret_cast<const uint8*>(file1.c_str()), file1.length());

  bool can_parse_header = info->ParseHeader();
  EXPECT_TRUE(can_parse_header);

  // The executable is the whole file, not 'embedded' with the file
  EXPECT_EQ(file1.length(), info->length());

  ExpectExecutable(info.get());
  EXPECT_EQ(449536, info->size_of_code());
  EXPECT_EQ(SectionName(info->RVAToSection(0x00401234 - 0x00400000)),
            std::string(".text"));

  EXPECT_EQ(0, info->RVAToFileOffset(0));
  EXPECT_EQ(1024, info->RVAToFileOffset(4096));
  EXPECT_EQ(46928, info->RVAToFileOffset(50000));

  std::vector<courgette::RVA> relocs;
  bool can_parse_relocs = info->ParseRelocs(&relocs);
  EXPECT_TRUE(can_parse_relocs);

  const uint8* p = info->RVAToPointer(0);
  EXPECT_EQ(reinterpret_cast<const void*>(file1.c_str()),
            reinterpret_cast<const void*>(p));
  EXPECT_EQ('M', p[0]);
  EXPECT_EQ('Z', p[1]);
}

void ImageInfoTest::TestResourceDll() const {
  std::string file1 = FileContents("en-US.dll");

  scoped_ptr<courgette::PEInfo> info(new courgette::PEInfo());
  info->Init(reinterpret_cast<const uint8*>(file1.c_str()), file1.length());

  bool can_parse_header = info->ParseHeader();
  EXPECT_TRUE(can_parse_header);

  // The executable is the whole file, not 'embedded' with the file
  EXPECT_EQ(file1.length(), info->length());

  EXPECT_TRUE(info->ok());
  EXPECT_FALSE(info->has_text_section());
  EXPECT_EQ(0u, info->size_of_code());
}

TEST_F(ImageInfoTest, All) {
  TestExe();
  TestResourceDll();
}
