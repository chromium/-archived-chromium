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

#include "sandbox/sandbox_poc/pocdll/exports.h"
#include "sandbox/sandbox_poc/pocdll/utils.h"

// This file contains the tests used to verify the security of the file system.

// Tries to open a file and outputs the result.
// "path" can contain environment variables.
// "output" is the stream for the logging.
void TryOpenFile(wchar_t *path, FILE *output) {
  wchar_t path_expanded[MAX_PATH] = {0};
  DWORD size = ::ExpandEnvironmentStrings(path, path_expanded, MAX_PATH - 1);
  if (!size) {
    fprintf(output, "[ERROR] Cannot expand \"%S\". Error %S.\r\n", path,
            ::GetLastError());
  }

  HANDLE file;
  file = ::CreateFile(path_expanded,
                      GENERIC_READ,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      NULL,  // No security attributes
                      OPEN_EXISTING,
                      FILE_FLAG_BACKUP_SEMANTICS,
                      NULL);  // No template

  if (file && INVALID_HANDLE_VALUE != file) {
    fprintf(output, "[GRANTED] Opening file \"%S\". Handle 0x%p\r\n", path,
            file);
    ::CloseHandle(file);
  } else {
    fprintf(output, "[BLOCKED] Opening file \"%S\". Error %d.\r\n", path,
            ::GetLastError());
  }
}

void POCDLL_API TestFileSystem(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");

  TryOpenFile(L"%SystemDrive%", output);
  TryOpenFile(L"%SystemRoot%", output);
  TryOpenFile(L"%ProgramFiles%", output);
  TryOpenFile(L"%SystemRoot%\\System32", output);
  TryOpenFile(L"%SystemRoot%\\explorer.exe", output);
  TryOpenFile(L"%SystemRoot%\\Cursors\\arrow_i.cur", output);
  TryOpenFile(L"%AllUsersProfile%", output);
  TryOpenFile(L"%UserProfile%", output);
  TryOpenFile(L"%Temp%", output);
  TryOpenFile(L"%AppData%", output);
}
