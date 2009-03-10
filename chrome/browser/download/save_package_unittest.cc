// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/download/save_package.h"
#include "testing/gtest/include/gtest/gtest.h"

#define FPL FILE_PATH_LITERAL

namespace {

// This constant copied from save_package.cc.
#if defined(OS_WIN)
const uint32 kMaxFilePathLength = MAX_PATH - 1;
#elif defined(OS_POSIX)
const uint32 kMaxFilePathLength = PATH_MAX - 1;
#endif

// Used to make long filenames.
std::string long_file_name(
    "EFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz01234567"
    "89ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz012345"
    "6789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz0123"
    "456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789a");

bool HasOrdinalNumber(const FilePath::StringType& filename) {
  FilePath::StringType::size_type r_paren_index = filename.rfind(FPL(')'));
  FilePath::StringType::size_type l_paren_index = filename.rfind(FPL('('));
  if (l_paren_index >= r_paren_index)
    return false;

  for (FilePath::StringType::size_type i = l_paren_index + 1;
       i != r_paren_index; ++i) {
    if (!IsAsciiDigit(filename[i]))
      return false;
  }

  return true;
}

}  // namespace

class SavePackageTest : public testing::Test {
 public:
  SavePackageTest() {
    FilePath test_dir;
    PathService::Get(base::DIR_TEMP, &test_dir);

    save_package_success_ = new SavePackage(
        test_dir.AppendASCII("testfile.htm"),
        test_dir.AppendASCII("testfile_files"));

    // We need to construct a path that is *almost* kMaxFilePathLength long
    long_file_name.resize(kMaxFilePathLength + long_file_name.length());
    while (long_file_name.length() < kMaxFilePathLength)
      long_file_name += long_file_name;
    long_file_name.resize(kMaxFilePathLength - 9 - test_dir.value().length());

    save_package_fail_ = new SavePackage(
        test_dir.AppendASCII(long_file_name + ".htm"),
        test_dir.AppendASCII(long_file_name + "_files"));
  }

  bool GetGeneratedFilename(bool need_success_generate_filename,
                            const std::string& disposition,
                            const std::string& url,
                            bool need_htm_ext,
                            FilePath::StringType* generated_name) {
    SavePackage* save_package;
    if (need_success_generate_filename)
      save_package = save_package_success_.get();
    else
      save_package = save_package_fail_.get();
    return save_package->GenerateFilename(disposition, GURL(url), need_htm_ext,
                                          generated_name);
  }

 private:
  // SavePackage for successfully generating file name.
  scoped_refptr<SavePackage> save_package_success_;
  // SavePackage for failed generating file name.
  scoped_refptr<SavePackage> save_package_fail_;

  DISALLOW_COPY_AND_ASSIGN(SavePackageTest);
};

static const struct {
  const char* disposition;
  const char* url;
  const FilePath::CharType* expected_name;
  bool need_htm_ext;
} kGeneratedFiles[] = {
  // We mainly focus on testing duplicated names here, since retrieving file
  // name from disposition and url has been tested in DownloadManagerTest.

  // No useful information in disposition or URL, use default.
  {"1.html", "http://www.savepage.com/", FPL("saved_resource.htm"), true},

  // No duplicate occurs.
  {"filename=1.css", "http://www.savepage.com", FPL("1.css"), false},

  // No duplicate occurs.
  {"filename=1.js", "http://www.savepage.com", FPL("1.js"), false},

  // Append numbers for duplicated names.
  {"filename=1.css", "http://www.savepage.com", FPL("1(1).css"), false},

  // No duplicate occurs.
  {"filename=1(1).js", "http://www.savepage.com", FPL("1(1).js"), false},

  // Append numbers for duplicated names.
  {"filename=1.css", "http://www.savepage.com", FPL("1(2).css"), false},

  // Change number for duplicated names.
  {"filename=1(1).css", "http://www.savepage.com", FPL("1(3).css"), false},

  // No duplicate occurs.
  {"filename=1(11).css", "http://www.savepage.com", FPL("1(11).css"), false},
};

TEST_F(SavePackageTest, TestSuccessfullyGenerateSavePackageFilename) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kGeneratedFiles); ++i) {
    FilePath::StringType file_name;
    bool ok = GetGeneratedFilename(true,
                                   kGeneratedFiles[i].disposition,
                                   kGeneratedFiles[i].url,
                                   kGeneratedFiles[i].need_htm_ext,
                                   &file_name);
    ASSERT_TRUE(ok);
    EXPECT_EQ(kGeneratedFiles[i].expected_name, file_name);
  }
}

TEST_F(SavePackageTest, TestUnSuccessfullyGenerateSavePackageFilename) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kGeneratedFiles); ++i) {
    FilePath::StringType file_name;
    bool ok = GetGeneratedFilename(false,
                                   kGeneratedFiles[i].disposition,
                                   kGeneratedFiles[i].url,
                                   kGeneratedFiles[i].need_htm_ext,
                                   &file_name);
    ASSERT_FALSE(ok);
  }
}

TEST_F(SavePackageTest, TestLongSavePackageFilename) {
  const std::string base_url("http://www.google.com/");
  const std::string long_file = long_file_name + ".css";
  const std::string url = base_url + long_file;

  FilePath::StringType filename;
  // Test that the filename is successfully shortened to fit.
  ASSERT_TRUE(GetGeneratedFilename(true, "", url, false, &filename));
  EXPECT_TRUE(filename.length() < long_file.length());
  EXPECT_FALSE(HasOrdinalNumber(filename));

  // Test that the filename is successfully shortened to fit, and gets an
  // an ordinal appended.
  ASSERT_TRUE(GetGeneratedFilename(true, "", url, false, &filename));
  EXPECT_TRUE(filename.length() < long_file.length());
  EXPECT_TRUE(HasOrdinalNumber(filename));

  // Test that the filename is successfully shortened to fit, and gets a
  // different ordinal appended.
  FilePath::StringType filename2;
  ASSERT_TRUE(GetGeneratedFilename(true, "", url, false, &filename2));
  EXPECT_TRUE(filename2.length() < long_file.length());
  EXPECT_TRUE(HasOrdinalNumber(filename2));
  EXPECT_NE(filename, filename2);
}
