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

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/file_version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FileVersionInfoTest : public testing::Test {
};

std::wstring GetTestDataPath() {
  std::wstring path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  file_util::AppendToPath(&path, L"base");
  file_util::AppendToPath(&path, L"data");
  file_util::AppendToPath(&path, L"file_version_info_unittest");
  return path;
}

}

TEST(FileVersionInfoTest, HardCodedProperties) {
  const wchar_t* kDLLNames[] = {
    L"FileVersionInfoTest1.dll"
  };

  const wchar_t* kExpectedValues[1][15] = {
      // FileVersionInfoTest.dll
      L"Goooooogle",                      // company_name
      L"Google",                          // company_short_name
      L"This is the product name",        // product_name
      L"This is the product short name",  // product_short_name
      L"The Internal Name",               // internal_name
      L"4.3.2.1",                         // product_version
      L"Private build property",          // private_build
      L"Special build property",          // special_build
      L"This is a particularly interesting comment",  // comments
      L"This is the original filename",   // original_filename
      L"This is my file description",     // file_description
      L"1.2.3.4",                         // file_version
      L"This is the legal copyright",     // legal_copyright
      L"This is the legal trademarks",    // legal_trademarks
      L"This is the last change",         // last_change

  };

  for (int i = 0; i < arraysize(kDLLNames); ++i) {
    std::wstring dll_path = GetTestDataPath();
    file_util::AppendToPath(&dll_path, kDLLNames[i]);

    scoped_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfo(dll_path));

    int j = 0;
    EXPECT_EQ(kExpectedValues[i][j++], version_info->company_name());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->company_short_name());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->product_name());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->product_short_name());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->internal_name());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->product_version());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->private_build());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->special_build());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->comments());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->original_filename());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->file_description());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->file_version());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->legal_copyright());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->legal_trademarks());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->last_change());
  }
}

TEST(FileVersionInfoTest, IsOfficialBuild) {
  const wchar_t* kDLLNames[] = {
    L"FileVersionInfoTest1.dll",
    L"FileVersionInfoTest2.dll"
  };

  const bool kExpected[] = {
    true,
    false,
  };

  // Test consistency check.
  ASSERT_EQ(arraysize(kDLLNames), arraysize(kExpected));

  for (int i = 0; i < arraysize(kDLLNames); ++i) {
    std::wstring dll_path = GetTestDataPath();
    file_util::AppendToPath(&dll_path, kDLLNames[i]);

    scoped_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfo(dll_path));

    EXPECT_EQ(kExpected[i], version_info->is_official_build());
  }
}

TEST(FileVersionInfoTest, CustomProperties) {
  std::wstring dll_path = GetTestDataPath();
  file_util::AppendToPath(&dll_path, L"FileVersionInfoTest1.dll");

  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(dll_path));

  // Test few existing properties.
  std::wstring str;
  EXPECT_TRUE(version_info->GetValue(L"Custom prop 1",  &str));
  EXPECT_EQ(L"Un", str);
  EXPECT_EQ(L"Un", version_info->GetStringValue(L"Custom prop 1"));

  EXPECT_TRUE(version_info->GetValue(L"Custom prop 2",  &str));
  EXPECT_EQ(L"Deux", str);
  EXPECT_EQ(L"Deux", version_info->GetStringValue(L"Custom prop 2"));

  EXPECT_TRUE(version_info->GetValue(L"Custom prop 3",  &str));
  EXPECT_EQ(L"1600 Amphitheatre Parkway Mountain View, CA 94043", str);
  EXPECT_EQ(L"1600 Amphitheatre Parkway Mountain View, CA 94043",
            version_info->GetStringValue(L"Custom prop 3"));

  // Test an non-existing property.
  EXPECT_FALSE(version_info->GetValue(L"Unknown property",  &str));
  EXPECT_EQ(L"", version_info->GetStringValue(L"Unknown property"));
}
