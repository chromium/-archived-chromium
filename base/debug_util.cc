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

#include "base/basictypes.h"
#include "base/debug_util.h"

namespace {

// Minimalist key reader.
// Note: Does not use the CRT.
bool RegReadString(HKEY root, const wchar_t* subkey,
                   const wchar_t* value_name, wchar_t* buffer, int* len) {
  HKEY key = NULL;
  DWORD res = RegOpenKeyEx(root, subkey, 0, KEY_READ, &key);
  if (ERROR_SUCCESS != res || key == NULL)
    return false;

  DWORD type = 0;
  DWORD buffer_size = *len * sizeof(wchar_t);
  // We don't support REG_EXPAND_SZ.
  res = RegQueryValueEx(key, value_name, NULL, &type,
                        reinterpret_cast<BYTE*>(buffer), &buffer_size);
  if (ERROR_SUCCESS == res && buffer_size != 0 && type == REG_SZ) {
    // Make sure the buffer is NULL terminated.
    buffer[*len - 1] = 0;
    *len = lstrlen(buffer);
    RegCloseKey(key);
    return true;
  }
  RegCloseKey(key);
  return false;
}

// Replaces each "%ld" in input per a value. Not efficient but it works.
// Note: Does not use the CRT.
bool StringReplace(const wchar_t* input, int value, wchar_t* output,
                   int output_len) {
  memset(output, 0, output_len*sizeof(wchar_t));
  int input_len = lstrlen(input);

  for (int i = 0; i < input_len; ++i) {
    int current_output_len = lstrlen(output);

    if (input[i] == L'%' && input[i + 1] == L'l' && input[i + 2] == L'd') {
      // Make sure we have enough place left.
      if ((current_output_len + 12) >= output_len)
        return false;

      // Cheap _itow().
      wsprintf(output+current_output_len, L"%d", value);
      i += 2;
    } else {
      if (current_output_len >= output_len)
        return false;
      output[current_output_len] = input[i];
    }
  }
  return true;
}

}  // namespace

// Note: Does not use the CRT.
bool DebugUtil::SpawnDebuggerOnProcess(unsigned process_id) {
  wchar_t reg_value[1026];
  int len = arraysize(reg_value);
  if (RegReadString(HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug",
      L"Debugger", reg_value, &len)) {
    wchar_t command_line[1026];
    if (StringReplace(reg_value, process_id, command_line,
                      arraysize(command_line))) {
      // We don't mind if the debugger is present because it will simply fail
      // to attach to this process.
      STARTUPINFO startup_info = {0};
      startup_info.cb = sizeof(startup_info);
      PROCESS_INFORMATION process_info = {0};

      if (CreateProcess(NULL, command_line, NULL, NULL, FALSE, 0, NULL, NULL,
                        &startup_info, &process_info)) {
        CloseHandle(process_info.hThread);
        WaitForInputIdle(process_info.hProcess, 10000);
        CloseHandle(process_info.hProcess);
        return true;
      }
    }
  }
  return false;
}

// Note: Does not use the CRT.
bool DebugUtil::WaitForDebugger(int wait_seconds, bool silent) {
  for (int i = 0; i < wait_seconds * 10; ++i) {
    if (IsDebuggerPresent()) {
      if (!silent) {
        // If you hit here, you just use -debug-on-start or -debug-children.
        __debugbreak();
      }
      return true;
    }
    Sleep(100);
  }
  return false;
}
