// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
