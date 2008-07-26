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

#ifndef CHROME_BROWSER_PRINTING_PRINTING_TEST_H__
#define CHROME_BROWSER_PRINTING_PRINTING_TEST_H__

#include <windows.h>
#include <winspool.h>

// Disable the whole test case when executing on a computer that has no printer
// installed.
// Note: Parent should be testing::Test or UITest.
template<typename Parent>
class PrintingTest : public Parent {
 public:
  static bool IsTestCaseDisabled() {
    return GetDefaultPrinter().empty();
  }
  static std::wstring GetDefaultPrinter() {
    wchar_t printer_name[MAX_PATH];
    DWORD size = arraysize(printer_name);
    BOOL result = ::GetDefaultPrinter(printer_name, &size);
    if (result == 0) {
      if (GetLastError() == ERROR_FILE_NOT_FOUND) {
        printf("There is no printer installed, printing can't be tested!\n");
        return std::wstring();
      }
      printf("INTERNAL PRINTER ERROR!\n");
      return std::wstring();
    }
    return printer_name;
  }
};

#endif  // CHROME_BROWSER_PRINTING_PRINTING_TEST_H__
