// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "keyboard_util.h"

void ClickKey(HWND hwnd, WORD key) {
  INPUT input[2];
  memset(&input, 0, sizeof(INPUT)*2);

  // Press key.
  input[0].type = INPUT_KEYBOARD;
  input[0].ki.wVk = key;
  input[0].ki.dwFlags = 0;

  // Release key.
  input[1].type = INPUT_KEYBOARD;
  input[1].ki.wVk = key;
  input[1].ki.dwFlags = KEYEVENTF_KEYUP;

  SendInput(2, input, sizeof(INPUT));
}

void ClickKey(HWND hwnd, WORD extended_key, WORD key) {
  INPUT input[4];
  memset(&input, 0, sizeof(INPUT)*4);

  // Press extended key.
  input[0].type = INPUT_KEYBOARD;
  input[0].ki.wVk = extended_key;
  input[0].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;

  // Press key.
  input[1].type = INPUT_KEYBOARD;
  input[1].ki.wVk = key;
  input[1].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;

  // Release key.
  input[2].type = INPUT_KEYBOARD;
  input[2].ki.wVk = key;
  input[2].ki.dwFlags = KEYEVENTF_EXTENDEDKEY|KEYEVENTF_KEYUP;

  // Release key.
  input[3].type = INPUT_KEYBOARD;
  input[3].ki.wVk = extended_key;
  input[3].ki.dwFlags = KEYEVENTF_EXTENDEDKEY|KEYEVENTF_KEYUP;

  SendInput(4, input, sizeof(INPUT));
}

void ClickKey(HWND hwnd, WORD extended_key1, WORD extended_key2, WORD key) {
  INPUT input[6];
  memset(&input, 0, sizeof(INPUT)*6);

  // Press extended key1.
  input[0].type = INPUT_KEYBOARD;
  input[0].ki.wVk = extended_key1;
  input[0].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;

  // Press extended key2.
  input[1].type = INPUT_KEYBOARD;
  input[1].ki.wVk = extended_key2;
  input[1].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;

  // Press key.
  input[2].type = INPUT_KEYBOARD;
  input[2].ki.wVk = key;
  input[2].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;

  // Release key.
  input[3].type = INPUT_KEYBOARD;
  input[3].ki.wVk = key;
  input[3].ki.dwFlags = KEYEVENTF_EXTENDEDKEY|KEYEVENTF_KEYUP;

  // Release extended key2.
  input[4].type       = INPUT_KEYBOARD;
  input[4].ki.wVk     = extended_key2;
  input[4].ki.dwFlags = KEYEVENTF_EXTENDEDKEY|KEYEVENTF_KEYUP;

  // Release extended key1.
  input[5].type = INPUT_KEYBOARD;
  input[5].ki.wVk = extended_key1;
  input[5].ki.dwFlags = KEYEVENTF_EXTENDEDKEY|KEYEVENTF_KEYUP;

  SendInput(6, input, sizeof(INPUT));
}

void PressKey(HWND hwnd, WORD key) {
  SendMessage(hwnd, WM_KEYDOWN, key, 0);
  SendMessage(hwnd, WM_CHAR, key, 0);
}

void ReleaseKey(HWND hwnd, WORD key) {
  SendMessage(hwnd, WM_KEYUP, key, 0);
}

KEYBD_KEYS GetKeybdKeysVal(BSTR str) {
  if (0 == wcslen(str))
    return KEY_INVALID;

  if (0 == _wcsicmp(str, L"F3"))
    return KEY_F3;
  if (0 == _wcsicmp(str, L"F4"))
    return KEY_F4;
  if (0 == _wcsicmp(str, L"F5"))
    return KEY_F5;
  if (0 == _wcsicmp(str, L"F6"))
    return KEY_F6;
  if (0 == _wcsicmp(str, L"ALT"))
    return KEY_ALT;
  if (0 == _wcsicmp(str, L"ALTER"))
    return KEY_ALT;
  if (0 == _wcsicmp(str, L"CTRL"))
    return KEY_CONTROL;
  if (0 == _wcsicmp(str, L"CONTROL"))
    return KEY_CONTROL;
  if (0 == _wcsicmp(str, L"SHIFT"))
    return KEY_SHIFT;
  if (0 == _wcsicmp(str, L"ENTER"))
    return KEY_ENTER;
  if (0 == _wcsicmp(str, L"RETURN"))
    return KEY_ENTER;
  if (0 == _wcsicmp(str, L"TAB"))
    return KEY_TAB;
  if (0 == _wcsicmp(str, L"BACK"))
    return KEY_BACK;
  if (0 == _wcsicmp(str, L"HOME"))
    return KEY_HOME;
  if (0 == _wcsicmp(str, L"END"))
    return KEY_END;
  if (0 == _wcsicmp(str, L"ESC"))
    return KEY_ESC;
  if (0 == _wcsicmp(str, L"ESCAPE"))
    return KEY_ESC;
  if (0 == _wcsicmp(str, L"INSERT"))
    return KEY_INSERT;
  if (0 == _wcsicmp(str, L"INS"))
    return KEY_INSERT;
  if (0 == _wcsicmp(str, L"DEL"))
    return KEY_DELETE;
  if (0 == _wcsicmp(str, L"DELETE"))
    return KEY_DELETE;
  if (0 == _wcsicmp(str, L"LEFT"))
    return KEY_LEFT;
  if (0 == _wcsicmp(str, L"RIGHT"))
    return KEY_RIGHT;
  if (0 == _wcsicmp(str, L"0"))
    return KEY_0;
  if (0 == _wcsicmp(str, L"1"))
    return KEY_1;
  if (0 == _wcsicmp(str, L"2"))
    return KEY_2;
  if (0 == _wcsicmp(str, L"3"))
    return KEY_3;
  if (0 == _wcsicmp(str, L"4"))
    return KEY_4;
  if (0 == _wcsicmp(str, L"5"))
    return KEY_5;
  if (0 == _wcsicmp(str, L"6"))
    return KEY_6;
  if (0 == _wcsicmp(str, L"7"))
    return KEY_7;
  if (0 == _wcsicmp(str, L"8"))
    return KEY_8;
  if (0 == _wcsicmp(str, L"9"))
    return KEY_9;
  if (0 == _wcsicmp(str, L"D"))
    return KEY_D;
  if (0 == _wcsicmp(str, L"F"))
    return KEY_F;
  if (0 == _wcsicmp(str, L"G"))
    return KEY_G;
  if (0 == _wcsicmp(str, L"K"))
    return KEY_K;
  if (0 == _wcsicmp(str, L"L"))
    return KEY_L;
  if (0 == _wcsicmp(str, L"N"))
    return KEY_N;
  if (0 == _wcsicmp(str, L"O"))
    return KEY_O;
  if (0 == _wcsicmp(str, L"R"))
    return KEY_R;
  if (0 == _wcsicmp(str, L"T"))
    return KEY_T;
  if (0 == _wcsicmp(str, L"W"))
    return KEY_W;
  if (0 == _wcsicmp(str, L"+"))
    return KEY_PLUS;
  if (0 == _wcsicmp(str, L"-"))
    return KEY_MINUS;

  // No key found.
  return KEY_INVALID;
}
