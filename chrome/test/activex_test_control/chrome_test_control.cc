// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
