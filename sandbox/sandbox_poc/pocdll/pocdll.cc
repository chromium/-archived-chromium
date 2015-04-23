// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "sandbox/sandbox_poc/pocdll/exports.h"
#include "sandbox/sandbox_poc/pocdll/utils.h"

BOOL APIENTRY DllMain(HMODULE module,
                      DWORD reason_for_call,
                      LPVOID reserved) {
  UNREFERENCED_PARAMETER(module);
  UNREFERENCED_PARAMETER(reason_for_call);
  UNREFERENCED_PARAMETER(reserved);
  return TRUE;
}

void POCDLL_API Run(HANDLE log) {
  TestFileSystem(log);
  TestRegistry(log);
  TestNetworkListen(log);
  TestSpyScreen(log);
  TestSpyKeys(log);
  TestThreads(log);
  TestProcesses(log);
  TestGetHandle(log);
}
