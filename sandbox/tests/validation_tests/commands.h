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

#ifndef SANDBOX_TESTS_VALIDATION_TESTS_COMMANDS_H__
#define SANDBOX_TESTS_VALIDATION_TESTS_COMMANDS_H__

namespace sandbox {

// Checks if window is a real window. Returns a SboxTestResult.
int TestValidWindow(HWND window);

// Tries to open the process_id. Returns a SboxTestResult.
int TestOpenProcess(DWORD process_id);

// Tries to open thread_id. Returns a SboxTestResult.
int TestOpenThread(DWORD thread_id);

// Tries to open path for read access. Returns a SboxTestResult.
int TestOpenReadFile(const std::wstring& path);

// Tries to open path for write access. Returns a SboxTestResult.
int TestOpenWriteFile(const std::wstring& path);

// Tries to open a registry key.
int TestOpenKey(HKEY base_key, std::wstring subkey);

// Tries to open the workstation's input desktop as long as the
// current desktop is not the interactive one. Returns a SboxTestResult.
int TestOpenInputDesktop();

}  // namespace sandbox

#endif  // SANDBOX_TESTS_VALIDATION_TESTS_COMMANDS_H__
