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


// This file contains the tests of the file path utils.
#include <stdio.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "tests/common/win/testing_common.h"
#include "utils/cross/file_path_utils.h"

namespace o3d {

class FilePathUtilsTest : public testing::Test {
};

TEST_F(FilePathUtilsTest, ConvertFilePathToUTF8) {
  std::string test_path("/this/is/a/path");
  FilePath source_path(FILE_PATH_LITERAL("/this/is/a/path"));
  EXPECT_EQ(test_path, FilePathToUTF8(source_path));
}

TEST_F(FilePathUtilsTest, ConvertFilePathToWide) {
  std::wstring test_path(L"/this/is/a/path");
  FilePath source_path(FILE_PATH_LITERAL("/this/is/a/path"));
  EXPECT_EQ(test_path, FilePathToWide(source_path));
}

TEST_F(FilePathUtilsTest, ConvertWideToFilePath) {
  std::wstring test_path(L"/this/is/a/path");
  FilePath dest_path = WideToFilePath(test_path);
  EXPECT_STREQ(FILE_PATH_LITERAL("/this/is/a/path"), dest_path.value().c_str());
}

TEST_F(FilePathUtilsTest, ConvertUTF8ToFilePath) {
  std::string test_path("/this/is/a/path");
  FilePath dest_path = UTF8ToFilePath(test_path);
  EXPECT_STREQ(FILE_PATH_LITERAL("/this/is/a/path"), dest_path.value().c_str());
}

TEST_F(FilePathUtilsTest, AbsolutePathBasic) {
  FilePath cwd;
  file_util::GetCurrentDirectory(&cwd);
#if defined(OS_WIN)
  FilePath test_path(FILE_PATH_LITERAL("this\\is\\a\\path"));
#else
  FilePath test_path(FILE_PATH_LITERAL("this/is/a/path"));
#endif
  FilePath abs_path = test_path;
  bool result = AbsolutePath(&abs_path);
  FilePath expected_result = cwd;
  expected_result = expected_result.Append(test_path);
  EXPECT_STREQ(expected_result.value().c_str(), abs_path.value().c_str());
}

TEST_F(FilePathUtilsTest, AbsolutePathAlreadyAbsolute) {
#if defined(OS_WIN)
  FilePath test_path(FILE_PATH_LITERAL("c:\\this\\is\\a\\path"));
#else
  FilePath test_path(FILE_PATH_LITERAL("/this/is/a/path"));
#endif
  FilePath abs_path = test_path;
  bool result = AbsolutePath(&abs_path);
  EXPECT_STREQ(test_path.value().c_str(), abs_path.value().c_str());
}

#if defined(OS_WIN)
TEST_F(FilePathUtilsTest, AbsolutePathAlreadyAbsoluteWindowsUnc) {
  FilePath test_path(FILE_PATH_LITERAL("\\\\this\\is\\a\\path"));
  FilePath abs_path = test_path;
  bool result = AbsolutePath(&abs_path);
  EXPECT_STREQ(test_path.value().c_str(), abs_path.value().c_str());
}
#endif

TEST_F(FilePathUtilsTest, RelativePathsBasic) {
#if defined(OS_WIN)
  FilePath expected_result(FILE_PATH_LITERAL("under\\parent"));
#else
  FilePath expected_result(FILE_PATH_LITERAL("under/parent"));
#endif
  FilePath base_path(FILE_PATH_LITERAL("/this/is/a/path"));
  FilePath child_path(FILE_PATH_LITERAL("/this/is/a/path/under/parent"));
  FilePath result;
  bool is_relative = GetRelativePathIfPossible(base_path, child_path, &result);
  EXPECT_STREQ(expected_result.value().c_str(), result.value().c_str());
  EXPECT_TRUE(is_relative);
}

#if defined(OS_WIN)
TEST_F(FilePathUtilsTest, RelativePathsWindowsAbsolute) {
  FilePath expected_result(FILE_PATH_LITERAL("under\\parent"));
  FilePath base_path(FILE_PATH_LITERAL("c:\\this\\is\\a\\path"));
  FilePath child_path(
      FILE_PATH_LITERAL("c:\\this\\is\\a\\path\\under\\parent"));
  FilePath result;
  bool is_relative = GetRelativePathIfPossible(base_path, child_path, &result);
  EXPECT_STREQ(expected_result.value().c_str(), result.value().c_str());
  EXPECT_TRUE(is_relative);
}

TEST_F(FilePathUtilsTest, RelativePathsWindowsDifferentDrives) {
  FilePath base_path(FILE_PATH_LITERAL("c:\\this\\is\\a\\path"));
  FilePath child_path(
      FILE_PATH_LITERAL("d:\\this\\is\\a\\path\\not\\under\\parent"));
  FilePath result;
  bool is_relative = GetRelativePathIfPossible(base_path, child_path, &result);
  EXPECT_STREQ(child_path.value().c_str(), result.value().c_str());
  EXPECT_FALSE(is_relative);
}
#endif

TEST_F(FilePathUtilsTest, RelativePathsCaseDifferent) {
  FilePath base_path(FILE_PATH_LITERAL("/This/Is/A/Path"));
  FilePath child_path(FILE_PATH_LITERAL("/this/is/a/path/under/parent"));
  FilePath result;
  bool is_relative = GetRelativePathIfPossible(base_path, child_path, &result);
#if defined(OS_WIN)
  EXPECT_STREQ(FILE_PATH_LITERAL("under\\parent"), result.value().c_str());
  EXPECT_TRUE(is_relative);
#else
  EXPECT_STREQ(child_path.value().c_str(), result.value().c_str());
  EXPECT_FALSE(is_relative);
#endif
}

TEST_F(FilePathUtilsTest, RelativePathsTrailingSlash) {
#if defined(OS_WIN)
  FilePath expected_result(FILE_PATH_LITERAL("under\\parent"));
#else
  FilePath expected_result(FILE_PATH_LITERAL("under/parent"));
#endif
  FilePath base_path(FILE_PATH_LITERAL("/this/is/a/path/"));
  FilePath child_path(FILE_PATH_LITERAL("/this/is/a/path/under/parent"));
  FilePath result;
  bool is_relative = GetRelativePathIfPossible(base_path, child_path, &result);
  EXPECT_STREQ(expected_result.value().c_str(), result.value().c_str());
  EXPECT_TRUE(is_relative);
}

TEST_F(FilePathUtilsTest, RelativePathsRelativeInputs) {
#if defined(OS_WIN)
  FilePath expected_result(FILE_PATH_LITERAL("under\\parent"));
#else
  FilePath expected_result(FILE_PATH_LITERAL("under/parent"));
#endif
  FilePath base_path(FILE_PATH_LITERAL("this/is/a/path"));
  FilePath child_path(FILE_PATH_LITERAL("this/is/a/path/under/parent"));
  FilePath result;
  bool is_relative = GetRelativePathIfPossible(base_path, child_path, &result);
  EXPECT_STREQ(expected_result.value().c_str(), result.value().c_str());
  EXPECT_TRUE(is_relative);
}
}  // namespace o3d
