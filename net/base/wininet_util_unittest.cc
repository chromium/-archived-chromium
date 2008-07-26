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

#include <windows.h>
#include <wininet.h>

#include "net/base/net_errors.h"
#include "net/base/wininet_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::WinInetUtil;

namespace {
  class WinInetUtilTest : public testing::Test {
  };
}

TEST(WinInetUtilTest, ErrorCodeConversion) {
  // a list of Windows error codes and the corresponding
  // net::ERR_xxx error codes
  static const struct {
    DWORD os_error;
    int net_error;
  } error_cases[] = {
    {ERROR_SUCCESS, net::OK},
    {ERROR_IO_PENDING, net::ERR_IO_PENDING},
    {ERROR_INTERNET_OPERATION_CANCELLED, net::ERR_ABORTED},
    {ERROR_INTERNET_CANNOT_CONNECT, net::ERR_CONNECTION_FAILED},
    {ERROR_INTERNET_NAME_NOT_RESOLVED, net::ERR_NAME_NOT_RESOLVED},
    {ERROR_INTERNET_INVALID_CA, net::ERR_CERT_AUTHORITY_INVALID},
    {999999, net::ERR_FAILED},
  };

  for (int i = 0; i < arraysize(error_cases); i++) {
    EXPECT_EQ(error_cases[i].net_error,
              WinInetUtil::OSErrorToNetError(error_cases[i].os_error));
  }
}
