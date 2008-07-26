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

#include "chrome/common/gfx/emf.h"

// For quick access.
#include <wingdi.h>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/printing/win_printing_context.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This test is automatically disabled if no printer named "UnitTest Printer" is
// available.
class EmfPrintingTest : public testing::Test {
 public:
  typedef testing::Test Parent;
  static bool IsTestCaseDisabled() {
    // It is assumed this printer is a HP Color LaserJet 4550 PCL or 4700.
    HDC hdc = CreateDC(L"WINSPOOL", L"UnitTest Printer", NULL, NULL);
    if (!hdc)
      return true;
    DeleteDC(hdc);
    return false;
  }
};

}  // namespace

TEST(EmfTest, DC) {
  static const int EMF_HEADER_SIZE = 128;

  // Simplest use case.
  gfx::Emf emf;
  RECT rect = {100, 100, 200, 200};
  HDC hdc = CreateCompatibleDC(NULL);
  EXPECT_TRUE(hdc != NULL);
  EXPECT_TRUE(emf.CreateDc(hdc, &rect));
  EXPECT_TRUE(emf.hdc() != NULL);
  // In theory, you'd use the HDC with GDI functions here.
  EXPECT_TRUE(emf.CloseDc());
  unsigned size = emf.GetDataSize();
  EXPECT_EQ(size, EMF_HEADER_SIZE);
  std::vector<BYTE> data;
  EXPECT_TRUE(emf.GetData(&data));
  EXPECT_EQ(data.size(), size);
  emf.CloseEmf();
  EXPECT_TRUE(DeleteDC(hdc));

  // Playback the data.
  hdc = CreateCompatibleDC(NULL);
  EXPECT_TRUE(hdc);
  EXPECT_TRUE(emf.CreateFromData(&data.front(), size));
  RECT output_rect = {0, 0, 10, 10};
  EXPECT_TRUE(emf.Playback(hdc, &output_rect));
  EXPECT_TRUE(DeleteDC(hdc));
}


// Disabled if no "UnitTest printer" exist. Useful to reproduce bug 1186598.
TEST_F(EmfPrintingTest, Enumerate) {
  if (IsTestCaseDisabled())
    return;

  printing::PrintSettings settings;

  // My test case is a HP Color LaserJet 4550 PCL.
  settings.set_device_name(L"UnitTest Printer");

  // Initialize it.
  printing::PrintingContext context;
  EXPECT_EQ(context.InitWithSettings(settings), printing::PrintingContext::OK);

  std::wstring test_file;
  PathService::Get(chrome::DIR_TEST_DATA, &test_file);

  // Load any EMF with an image.
  gfx::Emf emf;
  file_util::AppendToPath(&test_file, L"printing");
  file_util::AppendToPath(&test_file, L"test4.emf");
  std::string emf_data;
  file_util::ReadFileToString(test_file, &emf_data);
  ASSERT_TRUE(emf_data.size());
  EXPECT_TRUE(emf.CreateFromData(&emf_data[0], emf_data.size()));

  // This will print to file. The reason is that when running inside a
  // unit_test, printing::PrintingContext automatically dumps its files to the
  // current directory.
  // TODO(maruel):  Clean the .PRN file generated in current directory.
  context.NewDocument(L"EmfTest.Enumerate");
  context.NewPage();
  // Process one at a time.
  gfx::Emf::Enumerator emf_enum(emf, context.context(),
                                &emf.GetBounds().ToRECT());
  for (gfx::Emf::Enumerator::const_iterator itr = emf_enum.begin();
       itr != emf_enum.end();
       ++itr) {
    // To help debugging.
    ptrdiff_t index = itr - emf_enum.begin();
    // If you get this assert, you need to lookup iType in wingdi.h. It starts
    // with EMR_HEADER.
    EMR_HEADER;
    EXPECT_TRUE(itr->SafePlayback(NULL)) <<
        " index: " << index << " type: " << itr->record()->iType;
  }
  context.PageDone();
  context.DocumentDone();
}
