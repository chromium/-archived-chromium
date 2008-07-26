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

#ifndef CHROME_TEST_ACCISSIBILITY_KEYBOARD_UTIL_H__
#define CHROME_TEST_ACCISSIBILITY_KEYBOARD_UTIL_H__

#include "constants.h"

#include <oleauto.h>

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


#endif  // CHROME_TEST_ACCISSIBILITY_KEYBOARD_UTIL_H__
