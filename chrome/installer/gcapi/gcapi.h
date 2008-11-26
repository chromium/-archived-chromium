// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_GCAPI_GCAPI_H_
#define CHROME_INSTALLER_GCAPI_GCAPI_H_

#include <windows.h>

extern "C" {
// Error conditions for GoogleChromeCompatibilityCheck().
#define GCCC_ERROR_ALREADYPRESENT                0x01
#define GCCC_ERROR_ACCESSDENIED                  0x02
#define GCCC_ERROR_OSNOTSUPPORTED                0x04

#define DLLEXPORT __declspec(dllexport)

// This function returns TRUE if the Google Chrome should be offered.
// If the answer is FALSE, the reasons DWORD explains why.  If you don't care
// for the reason, you can pass NULL for reasons.
DLLEXPORT BOOL __stdcall GoogleChromeCompatibilityCheck(DWORD *reasons);

// Funtion pointer type declaration to use with GetProcAddress.
typedef BOOL (__stdcall * GCCC_FN)(HKEY, DWORD *);
}  // extern "C" 

#endif  // # CHROME_INSTALLER_GCAPI_GCAPI_H_
