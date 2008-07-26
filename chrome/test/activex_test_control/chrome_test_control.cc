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

#include "chrome/test/activex_test_control/chrome_test_control.h"

// CChromeTestControl
HRESULT ChromeTestControl::OnDraw(ATL_DRAWINFO& di) {
  RECT& rc = *(RECT*)di.prcBounds;
  // Set Clip region to the rectangle specified by di.prcBounds
  HRGN rgn_old = NULL;
  if (GetClipRgn(di.hdcDraw, rgn_old) != 1)
    rgn_old = NULL;
  bool select_old_rgn = false;

  HRGN rgn_new = CreateRectRgn(rc.left, rc.top, rc.right, rc.bottom);

  if (rgn_new != NULL)
    select_old_rgn = (SelectClipRgn(di.hdcDraw, rgn_new) != ERROR);

  Rectangle(di.hdcDraw, rc.left, rc.top, rc.right, rc.bottom);
  SetTextAlign(di.hdcDraw, TA_CENTER|TA_BASELINE);
  LPCTSTR pszText = _T("ATL 8.0 : ChromeTestControl");
  TextOut(di.hdcDraw,
          (rc.left + rc.right) / 2,
          (rc.top + rc.bottom) / 2,
          pszText,
          lstrlen(pszText));

  if (select_old_rgn)
    SelectClipRgn(di.hdcDraw, rgn_old);

  return S_OK;
}
