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
DLLEXPORT BOOL __stdcall GoogleChromeCompatibilityCheck(DWORD *reasons);

// This function launches Google Chrome after a successful install. If
// proc_handle is not NULL, the process handle of the newly created process
// will be returned.
DLLEXPORT BOOL __stdcall LaunchGoogleChrome(HANDLE* proc_handle);

// Funtion pointer type declarations to use with GetProcAddress.
typedef BOOL (__stdcall * GCCC_CompatibilityCheck)(DWORD *);
typedef BOOL (__stdcall * GCCC_LaunchGC)(HANDLE *);
}  // extern "C" 

#endif  // # CHROME_INSTALLER_GCAPI_GCAPI_H_
