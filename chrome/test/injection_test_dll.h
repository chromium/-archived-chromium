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
