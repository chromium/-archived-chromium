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

#include "testing/gtest/include/gtest/gtest.h"
#include "base/wmi_util.h"

TEST(WMIUtilTest, TestLocalConnectionSecurityBlanket) {
  ::CoInitialize(NULL);
  IWbemServices* services = NULL;
  EXPECT_TRUE(WMIUtil::CreateLocalConnection(true, &services));
  ASSERT_TRUE(NULL != services);
  ULONG refs = services->Release();
  EXPECT_EQ(refs, 0);
  ::CoUninitialize();
}

TEST(WMIUtilTest, TestLocalConnectionNoSecurityBlanket) {
  ::CoInitialize(NULL);
  IWbemServices* services = NULL;
  EXPECT_TRUE(WMIUtil::CreateLocalConnection(false, &services));
  ASSERT_TRUE(NULL != services);
  ULONG refs = services->Release();
  EXPECT_EQ(refs, 0);
  ::CoUninitialize();
}

TEST(WMIUtilTest, TestCreateClassMethod) {
  ::CoInitialize(NULL);
  IWbemServices* wmi_services = NULL;
  EXPECT_TRUE(WMIUtil::CreateLocalConnection(true, &wmi_services));
  ASSERT_TRUE(NULL != wmi_services);
  IWbemClassObject* class_method = NULL;
  EXPECT_TRUE(WMIUtil::CreateClassMethodObject(wmi_services,
                                               L"Win32_ShortcutFile",
                                               L"Rename", &class_method));
  ASSERT_TRUE(NULL != class_method);
  ULONG refs = class_method->Release();
  EXPECT_EQ(refs, 0);
  refs = wmi_services->Release();
  EXPECT_EQ(refs, 0);
  ::CoUninitialize();
}

// Creates an instance of cmd which executes 'echo' and exits immediately.
TEST(WMIUtilTest, TestLaunchProcess) {
  ::CoInitialize(NULL);
  int pid = 0;
  bool result = WMIProcessUtil::Launch(L"cmd.exe /c echo excelent!", &pid);
  EXPECT_TRUE(result);
  EXPECT_GT(pid, 0);
  ::CoUninitialize();
}
