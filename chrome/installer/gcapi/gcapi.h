// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_GCAPI_GCAPI_H_
#define CHROME_INSTALLER_GCAPI_GCAPI_H_

#include <windows.h>

extern "C" {
// Error conditions for GoogleChromeCompatibilityCheck().
#define GCCC_ERROR_USERLEVELALREADYPRESENT       0x01
#define GCCC_ERROR_SYSTEMLEVELALREADYPRESENT     0x02
#define GCCC_ERROR_ACCESSDENIED                  0x04
#define GCCC_ERROR_OSNOTSUPPORTED                0x08
#define GCCC_ERROR_ALREADYOFFERED                0x10
#define GCCC_ERROR_INTEGRITYLEVEL                0x20

#define DLLEXPORT __declspec(dllexport)

// This function returns TRUE if Google Chrome should be offered.
// If the return is FALSE, the reasons DWORD explains why.  If you don't care
// for the reason, you can pass NULL for reasons.
// set_flag indicates whether a flag should be set indicating that Chrome was
// offered within the last six months; if passed FALSE, this method will not
// set the flag even if Chrome can be offered.  If passed TRUE, this method 
// will set the flag only if Chrome can be offered.
DLLEXPORT BOOL __stdcall GoogleChromeCompatibilityCheck(BOOL set_flag, 
                                                        DWORD *reasons);

// This function launches Google Chrome after a successful install. Make
// sure COM library is NOT initalized before you call this function (so if
// you called CoInitialize, call CoUninitialize before calling this function).
DLLEXPORT BOOL __stdcall LaunchGoogleChrome();

// Funtion pointer type declarations to use with GetProcAddress.
typedef BOOL (__stdcall * GCCC_CompatibilityCheck)(BOOL, DWORD *);
typedef BOOL (__stdcall * GCCC_LaunchGC)(HANDLE *);
}  // extern "C" 

#endif  // # CHROME_INSTALLER_GCAPI_GCAPI_H_
