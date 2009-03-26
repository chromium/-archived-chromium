// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/l10n_util.h"
#include "chrome/common/l10n_util_win.h"

#include "base/win_util.h"

namespace l10n_util {

int GetExtendedStyles() {
  return GetTextDirection() == LEFT_TO_RIGHT ? 0 :
      WS_EX_LAYOUTRTL | WS_EX_RTLREADING;
}

int GetExtendedTooltipStyles() {
  return GetTextDirection() == LEFT_TO_RIGHT ? 0 : WS_EX_LAYOUTRTL;
}

void HWNDSetRTLLayout(HWND hwnd) {
  DWORD ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);

  // We don't have to do anything if the style is already set for the HWND.
  if (!(ex_style & WS_EX_LAYOUTRTL)) {
    ex_style |= WS_EX_LAYOUTRTL;
    ::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);

    // Right-to-left layout changes are not applied to the window immediately
    // so we should make sure a WM_PAINT is sent to the window by invalidating
    // the entire window rect.
    ::InvalidateRect(hwnd, NULL, true);
  }
}

bool IsLocaleSupportedByOS(const std::wstring& locale) {
  // Block Oriya on Windows XP.
  return !(LowerCaseEqualsASCII(locale, "or") &&
      win_util::GetWinVersion() < win_util::WINVERSION_VISTA);
}

}  // namespace l10n_util
