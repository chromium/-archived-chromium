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

#include <string>
#include "sandbox/sandbox_poc/pocdll/exports.h"
#include "sandbox/sandbox_poc/pocdll/utils.h"

// This file contains the tests used to verify the security of the system by
// using some spying techniques.

void POCDLL_API TestSpyKeys(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");

  fprintf(output, "[INFO] Logging keystrokes for 15 seconds\r\n");
  fflush(output);
  std::wstring logged;
  DWORD tick = ::GetTickCount() + 15000;
  while (tick > ::GetTickCount()) {
    for (int i = 0; i < 256; ++i) {
      if (::GetAsyncKeyState(i) & 1) {
        if (i >=  VK_SPACE && i <= 0x5A /*VK_Z*/) {
          logged.append(1, static_cast<wchar_t>(i));
        } else {
          logged.append(1, '?');
        }
      }
    }
  }

  if (logged.size()) {
    fprintf(output, "[GRANTED] Spyed keystrokes \"%S\"\r\n",
            logged.c_str());
  } else {
    fprintf(output, "[BLOCKED] Spyed keystrokes \"(null)\"\r\n");
  }
}

void POCDLL_API TestSpyScreen(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");

  HDC screen_dc = ::GetDC(NULL);
  COLORREF pixel_color = ::GetPixel(screen_dc, 0, 0);

  for (int x = 0; x < 10; ++x) {
    for (int y = 0; y < 10; ++y) {
      if (::GetPixel(screen_dc, x, y) != pixel_color) {
        fprintf(output, "[GRANTED] Read pixel on screen\r\n");
        return;
      }
    }
  }

  fprintf(output, "[BLOCKED] Read pixel on screen. Error = %d\r\n",
          ::GetLastError());
}
