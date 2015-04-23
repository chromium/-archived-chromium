// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INJECTION_TEST_DLL_H__
#define CHROME_TEST_INJECTION_TEST_DLL_H__

// This file defines the entry points for any DLL that can be loaded into the
// renderer or plugin process for the purposes of testing. The DLL code must
// define TEST_INJECTION_DLL so the entry point definitions cause the linker
// to generate exported functions.

const char kRenderTestCall[] = "RunRendererTests";
const char kPluginTestCall[] = "RunPluginTests";

extern "C" {
#ifdef TEST_INJECTION_DLL
BOOL extern __declspec(dllexport) __cdecl RunRendererTests(int* test_count);
BOOL extern __declspec(dllexport) __cdecl RunPluginTests(int* test_count);
#else
typedef BOOL (__cdecl *RunRendererTests)(int* test_count);
typedef BOOL (__cdecl *RunPluginTests)(int* test_count);
#endif
}

#endif  // CHROME_TEST_INJECTION_TEST_DLL_H__
