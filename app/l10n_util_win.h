// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_L10N_UTIL_WIN_H_
#define APP_L10N_UTIL_WIN_H_

#include <windows.h>

namespace l10n_util {

// Returns the locale-dependent extended window styles.
// This function is used for adding locale-dependent extended window styles
// (e.g. WS_EX_LAYOUTRTL, WS_EX_RTLREADING, etc.) when creating a window.
// Callers should OR this value into their extended style value when creating
// a window.
int GetExtendedStyles();

// TODO(xji):
// This is a temporary name, it will eventually replace GetExtendedStyles
int GetExtendedTooltipStyles();

// Give an HWND, this function sets the WS_EX_LAYOUTRTL extended style for the
// underlying window. When this style is set, the UI for the window is going to
// be mirrored. This is generally done for the UI of right-to-left languages
// such as Hebrew.
void HWNDSetRTLLayout(HWND hwnd);

// See http://blogs.msdn.com/oldnewthing/archive/2005/09/15/467598.aspx
// and  http://blogs.msdn.com/oldnewthing/archive/2006/06/26/647365.aspx
// as to why we need these three functions.

// Return true if the default font (we get from Windows) is not suitable
// to use in the UI of the current UI (e.g. Malayalam, Bengali). If
// override_font_family and font_size_scaler are not null, they'll be
// filled with the font family name and the size scaler.
bool NeedOverrideDefaultUIFont(std::wstring* override_font_family,
                               double* font_size_scaler);

// If the default UI font stored in |logfont| is not suitable, its family
// and size are replaced with those stored in the per-locale resource.
void AdjustUIFont(LOGFONT* logfont);

// If the font for a given window (pointed to by HWND) is not suitable for the
// UI in the current UI langauge, its family and size are replaced with those
// stored in the per-locale resource.
void AdjustUIFontForWindow(HWND hwnd);

}  // namespace l10n_util

#endif  // APP_L10N_UTIL_WIN_H_
