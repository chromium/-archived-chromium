// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// stdafx.h : Include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently.
// This file is in Microsoft coding style.

#ifndef MEDIA_PLAYER_STDAFX_H_
#define MEDIA_PLAYER_STDAFX_H_

// Change these values to use different versions.
#define _RICHEDIT_VER  0x0100

#pragma warning(disable: 4996)
// warning C4996: '_vswprintf': This function or variable may be unsafe.
// Consider using vswprintf_s instead. To disable deprecation,
// use _CRT_SECURE_NO_WARNINGS. See online help for details.

#include <atlbase.h>
#include <atlapp.h>

extern CAppModule g_module;

#include <atlwin.h>

#endif  // MEDIA_PLAYER_STDAFX_H_

