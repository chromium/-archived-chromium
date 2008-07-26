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

#include "chrome/browser/automation/ui_controls.h"

#include "base/logging.h"

// Methods private to this file

// Populate the INPUT structure with the appropriate keyboard event
// parameters required by SendInput
bool FillKeyboardInput(wchar_t key, INPUT* input, bool key_up) {
  memset(input, 0, sizeof(INPUT));
  input->type = INPUT_KEYBOARD;
  input->ki.wVk = static_cast<WORD>(key);
  input->ki.dwFlags = key_up ? KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP :
    KEYEVENTF_EXTENDEDKEY;

  return true;
}

// Send a key event (up/down)
bool SendKeyEvent(wchar_t key, bool up) {
  INPUT input = { 0 };

  if (!FillKeyboardInput(key, &input, up))
    return false;

  if (!::SendInput(1, &input, sizeof(INPUT)))
    return false;

  return true;
}

namespace ui_controls {

bool SendKeyPress(wchar_t key, bool control, bool shift, bool alt) {
  INPUT input[8] = { 0 }; // 8, assuming all the modifiers are activated

  int i = 0;
  if (control) {
    if (!FillKeyboardInput(VK_CONTROL, &input[i], false))
      return false;
    i++;
  }

  if (shift) {
    if (!FillKeyboardInput(VK_SHIFT, &input[i], false))
      return false;
    i++;
  }

  if (alt) {
    if (!FillKeyboardInput(VK_MENU, &input[i], false))
      return false;
    i++;
  }

  if (!FillKeyboardInput(key, &input[i], false))
    return false;
  i++;

  if (!FillKeyboardInput(key, &input[i], true))
    return false;
  i++;

  if (alt) {
    if (!FillKeyboardInput(VK_MENU, &input[i], true))
      return false;
    i++;
  }

  if (shift) {
    if (!FillKeyboardInput(VK_SHIFT, &input[i], true))
      return false;
    i++;
  }

  if (control) {
    if (!FillKeyboardInput(VK_CONTROL, &input[i], true))
      return false;
    i++;
  }


  unsigned int rv = ::SendInput(i, input, sizeof(INPUT));

  return rv == i;
}

bool SendKeyDown(wchar_t key) {
  return SendKeyEvent(key, false);
}

bool SendKeyUp(wchar_t key) {
  return SendKeyEvent(key, true);
}

bool SendMouseMove(long x, long y) {
  INPUT input = { 0 };

  int screen_width = ::GetSystemMetrics(SM_CXSCREEN) - 1;
  int screen_height  = ::GetSystemMetrics(SM_CYSCREEN) - 1;
  LONG pixel_x  = static_cast<LONG>(x * (65535.0f / screen_width));
  LONG pixel_y = static_cast<LONG>(y * (65535.0f / screen_height));

  input.type = INPUT_MOUSE;
  input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
  input.mi.dx = pixel_x;
  input.mi.dy = pixel_y;

  if (!::SendInput(1, &input, sizeof(INPUT)))
    return false;
  return true;
}

bool SendMouseClick(MouseButton type) {

  DWORD down_flags = MOUSEEVENTF_ABSOLUTE;
  DWORD up_flags = MOUSEEVENTF_ABSOLUTE;

  switch(type) {
    case LEFT:
      down_flags |= MOUSEEVENTF_LEFTDOWN;
      up_flags |= MOUSEEVENTF_LEFTUP;

      break;
    case MIDDLE:
      down_flags |= MOUSEEVENTF_MIDDLEDOWN;
      up_flags |= MOUSEEVENTF_MIDDLEUP;

      break;
    case RIGHT:
      down_flags |= MOUSEEVENTF_RIGHTDOWN;
      up_flags |= MOUSEEVENTF_RIGHTUP;

      break;
    default:
      NOTREACHED();
      return false;
  }

  INPUT input = { 0 };
  input.type = INPUT_MOUSE;
  input.mi.dwFlags = down_flags;
  if (!::SendInput(1, &input, sizeof(INPUT)))
    return false;

  input.mi.dwFlags = up_flags;
  if (!::SendInput(1, &input, sizeof(INPUT)))
    return false;

  return true;
}

}  // ui_controls
