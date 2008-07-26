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

#include "base/logging.h"
#include "chrome/browser/save_package.h"
#include "testing/gtest/include/gtest/gtest.h"

//    0         1         2         3         4         5         6
//    0123456789012345678901234567890123456789012345678901234567890123456789
static const wchar_t* const kLongFilePath =
    L"C:\\EFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz01234567"
    L"89ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz012345"
    L"6789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz0123"
    L"456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789a.htm";

static const wchar_t* const kLongDirPath =
    L"C:\\EFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz01234567"
    L"89ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz012345"
    L"6789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz0123"
    L"456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789a_files";

class SavePackageTest : public testing::Test {
 public:
  SavePackageTest() {
    save_package_success_ = new SavePackage(L"c:\\testfile.htm",
                                            L"c:\\testfile_files");
    save_package_fail_ = new SavePackage(kLongFilePath, kLongDirPath);
  }

  bool GetGeneratedFilename(bool need_success_generate_filename,
                            const std::string& disposition,
                            const std::wstring& url,
                            bool need_htm_ext,
                            std::wstring* generated_name) {
    SavePackage* save_package;
    if (need_success_generate_filename)
      save_package = save_package_success_.get();
    else
      save_package = save_package_fail_.get();
    return save_package->GenerateFilename(disposition, url, need_htm_ext,
                                          generated_name);
  }

 protected:
  DISALLOW_EVIL_CONSTRUCTORS(SavePackageTest);

 private:
  // SavePackage for successfully generating file name.
  scoped_refptr<SavePackage> save_package_success_;
  // SavePackage for failed generating file name.
  scoped_refptr<SavePackage> save_package_fail_;
};

static const struct {
  const char* disposition;
  const wchar_t* url;
  const wchar_t* expected_name;
  bool need_htm_ext;
} kGeneratedFiles[] = {
  // We mainly focus on testing duplicated names here, since retrieving file
  // name from disposition and url has been tested in DownloadManagerTest.

  // No useful information in disposition or URL, use default.
  {"1.html", L"http://www.savepage.com/", L"saved_resource.htm", true},

  // No duplicate occurs.
  {"filename=1.css", L"http://www.savepage.com", L"1.css", false},

  // No duplicate occurs.
  {"filename=1.js", L"http://www.savepage.com", L"1.js", false},

  // Append numbers for duplicated names.
  {"filename=1.css", L"http://www.savepage.com", L"1(1).css", false},

  // No duplicate occurs.
  {"filename=1(1).js", L"http://www.savepage.com", L"1(1).js", false},

  // Append numbers for duplicated names.
  {"filename=1.css", L"http://www.savepage.com", L"1(2).css", false},

  // Change number for duplicated names.
  {"filename=1(1).css", L"http://www.savepage.com", L"1(3).css", false},

  // No duplicate occurs.
  {"filename=1(11).css", L"http://www.savepage.com", L"1(11).css", false},

  // test length of file path is great than MAX_PATH(260 characters).
  {"",
  // 0         1         2         3         4         5         6
  // 0123456789012345678901234567890123456789012345678901234567890123456789
  //                      0         1         2         3         4
  //                      01234567890123456789012345678901234567890123456789
  L"http://www.google.com/ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmn"
  L"opqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijkl"
  L"mnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghij"
  L"klmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefgh"
  L"test.css",
  // 0         1         2         3         4         5         6
  // 0123456789012345678901234567890123456789012345678901234567890123456789
  L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz01234567"
  L"89ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz012345"
  L"6789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz0123"
  L"456789ABCDEFGHIJKLMNOPQRSTU.css",
  false},

  // test duplicated file name, its length of file path is great than
  // MAX_PATH(260 characters).
  {"",
  // 0         1         2         3         4         5         6
  // 0123456789012345678901234567890123456789012345678901234567890123456789
  //                      0         1         2         3         4
  //                      01234567890123456789012345678901234567890123456789
  L"http://www.google.com/ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmn"
  L"opqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijkl"
  L"mnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghij"
  L"klmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefgh"
  L"test.css",
  // 0         1         2         3         4         5         6
  // 0123456789012345678901234567890123456789012345678901234567890123456789
  L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz01234567"
  L"89ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz012345"
  L"6789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz0123"
  L"456789ABCDEFGHIJKLMNO(1).css",
  false},

  {"",
  // 0         1         2         3         4         5         6
  // 0123456789012345678901234567890123456789012345678901234567890123456789
  //                      0         1         2         3         4
  //                      01234567890123456789012345678901234567890123456789
  L"http://www.google.com/ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmn"
  L"opqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijkl"
  L"mnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghij"
  L"klmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefgh"
  L"test.css",
  // 0         1         2         3         4         5         6
  // 0123456789012345678901234567890123456789012345678901234567890123456789
  L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz01234567"
  L"89ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz012345"
  L"6789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz0123"
  L"456789ABCDEFGHIJKLMNO(2).css",
  false}
};

TEST_F(SavePackageTest, TestSuccessfullyGenerateSavePackageFilename) {
  for (int i = 0; i < arraysize(kGeneratedFiles); ++i) {
    std::wstring file_name;
    bool ok = GetGeneratedFilename(true,
                                   kGeneratedFiles[i].disposition,
                                   kGeneratedFiles[i].url,
                                   kGeneratedFiles[i].need_htm_ext,
                                   &file_name);
    ASSERT_TRUE(ok);
    EXPECT_EQ(file_name, kGeneratedFiles[i].expected_name);
  }
}

TEST_F(SavePackageTest, TestUnSuccessfullyGenerateSavePackageFilename) {
  for (int i = 0; i < arraysize(kGeneratedFiles); ++i) {
    std::wstring file_name;
    bool ok = GetGeneratedFilename(false,
                                   kGeneratedFiles[i].disposition,
                                   kGeneratedFiles[i].url,
                                   kGeneratedFiles[i].need_htm_ext,
                                   &file_name);
    ASSERT_FALSE(ok);
  }
}
