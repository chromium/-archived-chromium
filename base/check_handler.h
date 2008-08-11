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

#ifndef BASE_CHECK_HANDLER_H__
#define BASE_CHECK_HANDLER_H__

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/logging.h"

// This class allows temporary handling of assert firing. When a CHECK()
// or DCHECK() assertion happens it will in turn generate a SEH exception
// which can be can captured using a windows SEH hander __try .. _except
// block. One practical use of this class is for unit tests that make sure
// CHECK conditions are appropriately handled. For example:
//
//  TEST(TestGroup, VerifyAssert) {
//    CheckAssertHandler expect_exception;
//    __try {
//      MyClass object;                  // MyClass dtor will not be called.
//      Param some_bad_param;
//      object.Method(some_bad_param);   //  Should triggers a CHECK().
//      ADD_FAILURE();                   // If we get here the test failed.
//    } __except(EXCEPTION_EXECUTE_HANDLER) {
//      DWORD ecode = GetExceptionCode();
//      EXPECT_EQ(CheckAssertHandler::seh_exception_code(), ecode);
//    }
//  }
//
// You can put MyClass outside the __try block so its destructor will be
// called which could lead to a crash if the state of the object is
// corrupted by the CHECK you are testing. If that is the case you should
// fix the state of the object inside the __except block.
//
// Since the above code is Windows specific, two helper macros are provided
// that hide the implementation details. Using the macros the code becomes:
//
//  TEST(TestGroup, VerifyAssert) {
//    CHECK_HANDLER_BEGIN
//      MyClass object;                  // MyClass dtor will not be called.
//      Param some_bad_param;
//      object.Method(some_bad_param);   //  Should triggers a CHECK().
//    CHECK_HANDLER_END
//  }
//
// Depending on the compiler settings you might have issue this pragma arround
// the code that uses this class:
//   #pragma warning(disable: 4509)
// Which tells the compiler that is ok that some dtors will not be called.
//
// Create this object on the stack always.Do not create it inside the
// __try block itself or the dtor will never be called. Create only one
// on each scope.
//
// The key detail here is the RaiseException() call which transfers
// program control away from the code that caused the assertion and back
// into the _except block.

#if defined(OS_WIN)

class CheckAssertHandler {
 public:
  // Installs the assert handler. The dtor will remove the handler.
  CheckAssertHandler() {
    logging::SetLogAssertHandler(&CheckAssertHandler::LogAssertHandler);
  }
  ~CheckAssertHandler() {
    logging::SetLogAssertHandler(NULL);
  }
  static DWORD seh_exception_code() { return 0x1765413; }
 private:
  static void LogAssertHandler(const std::string&) {
    ::RaiseException(seh_exception_code(), 0, 0, NULL);
  }
};

#define CHECK_HANDLER_BEGIN \
  CheckAssertHandler chk_ex_handler; \
  __try {

#define CHECK_HANDLER_END \
    ADD_FAILURE(); \
  } __except(EXCEPTION_EXECUTE_HANDLER) { \
    DWORD ecode = GetExceptionCode(); \
    EXPECT_EQ(CheckAssertHandler::seh_exception_code(), ecode); \
  }

#else

// SEH exceptions only make sense on windows, they're meaningless everywhere 
// else.

#define CHECK_HANDLER_BEGIN // no-op
#define CHECK_HANDLER_END // no-op

#endif

#endif  // BASE_CHECK_HANDLER_H__
