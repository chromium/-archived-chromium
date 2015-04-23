// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/zip.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Make the test a PlatformTest to setup autorelease pools properly on Mac.
class ZipTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    PlatformTest::SetUp();

    ASSERT_TRUE(file_util::CreateNewTempDirectory(
        FILE_PATH_LITERAL("unzip_unittest_"), &test_dir_));

    FilePath zip_path(test_dir_);
    zip_contents_.insert(zip_path.AppendASCII("foo.txt"));
    zip_path = zip_path.AppendASCII("foo");
    zip_contents_.insert(zip_path);
    zip_contents_.insert(zip_path.AppendASCII("bar.txt"));
    zip_path = zip_path.AppendASCII("bar");
    zip_contents_.insert(zip_path);
    zip_contents_.insert(zip_path.AppendASCII("baz.txt"));
    zip_contents_.insert(zip_path.AppendASCII("quux.txt"));
  }

  virtual void TearDown() {
    PlatformTest::TearDown();
    // Clean up test directory
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  void TestUnzipFile(const FilePath::StringType& filename) {
    FilePath test_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    test_dir = test_dir.AppendASCII("zip");
    TestUnzipFile(test_dir.Append(filename));
  }

  void TestUnzipFile(const FilePath& path) {
    ASSERT_TRUE(file_util::PathExists(path)) << "no file " << path.value();
    ASSERT_TRUE(Unzip(path, test_dir_));

    file_util::FileEnumerator files(test_dir_, true,
        static_cast<file_util::FileEnumerator::FILE_TYPE>(
            file_util::FileEnumerator::FILES |
            file_util::FileEnumerator::DIRECTORIES));
    FilePath next_path = files.Next();
    size_t count = 0;
    while (!next_path.value().empty()) {
      if (next_path.value().find(FILE_PATH_LITERAL(".svn")) ==
          FilePath::StringType::npos) {
        EXPECT_EQ(zip_contents_.count(next_path), 1U) <<
            "Couldn't find " << next_path.value();
        count++;
      }
      next_path = files.Next();
    }
    EXPECT_EQ(count, zip_contents_.size());
  }

  // the path to temporary directory used to contain the test operations
  FilePath test_dir_;

  // hard-coded contents of a known zip file
  std::set<FilePath> zip_contents_;
};

TEST_F(ZipTest, Unzip) {
  TestUnzipFile(FILE_PATH_LITERAL("test.zip"));
}

TEST_F(ZipTest, UnzipUncompressed) {
  TestUnzipFile(FILE_PATH_LITERAL("test_nocompress.zip"));
}


TEST_F(ZipTest, Zip) {
  FilePath src_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &src_dir));
  src_dir = src_dir.AppendASCII("zip").AppendASCII("test");

  FilePath zip_file;
  ASSERT_TRUE(file_util::CreateNewTempDirectory(
      FILE_PATH_LITERAL("unzip_unittest_"), &zip_file));
  zip_file = zip_file.AppendASCII("out.zip");

  EXPECT_TRUE(Zip(src_dir, zip_file));

  TestUnzipFile(zip_file);

  EXPECT_TRUE(file_util::Delete(zip_file, false));
}

}  // namespace
