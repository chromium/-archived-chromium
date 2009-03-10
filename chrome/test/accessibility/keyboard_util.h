// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_ACCESSIBILITY_KEYBOARD_UTIL_H_
#define CHROME_TEST_ACCESSIBILITY_KEYBOARD_UTIL_H_

#include <wtypes.h>

#include "chrome/test/accessibility/constants.h"

//////////////////////////////////////////////////////
// Function declarations to automate keyboard events.
//////////////////////////////////////////////////////

// Enqueue keyboard message for a key. Currently, hwnd is not used.
void ClickKey(HWND hwnd, WORD key);

// Enqueue keyboard message for a combination of 2 keys. Currently, hwnd is
// not used.
void ClickKey(HWND hwnd, WORD key, WORD extended_key);

// Enqueue keyboard message for a combination of 2 keys.
// Currently, hwnd is not used.
void ClickKey(HWND hwnd, WORD key, WORD extended_key1, WORD extended_key2);

// Sends key-down message to window.
void PressKey(HWND hwnd, WORD key);

// Sends key-up message to window.
void ReleaseKey(HWND hwnd, WORD key);

// Returns native enum values for a key-string specified.
KEYBD_KEYS GetKeybdKeysVal(BSTR str);

#endif  // CHROME_TEST_ACCESSIBILITY_KEYBOARD_UTIL_H_
