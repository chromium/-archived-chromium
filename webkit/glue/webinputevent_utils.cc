// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/webinputevent_utils.h"

#include "KeyboardCodes.h"

#include "base/string_util.h"

using namespace WebCore;

std::string GetKeyIdentifierForWindowsKeyCode(unsigned short key_code) {
  switch (key_code) {
    case VKEY_MENU:
      return "Alt";
    case VKEY_CONTROL:
      return "Control";
    case VKEY_SHIFT:
      return "Shift";
    case VKEY_CAPITAL:
      return "CapsLock";
    case VKEY_LWIN:
    case VKEY_RWIN:
      return "Win";
    case VKEY_CLEAR:
      return "Clear";
    case VKEY_DOWN:
      return "Down";
    // "End"
    case VKEY_END:
      return "End";
    // "Enter"
    case VKEY_RETURN:
      return "Enter";
    case VKEY_EXECUTE:
      return "Execute";
    case VKEY_F1:
      return "F1";
    case VKEY_F2:
      return "F2";
    case VKEY_F3:
      return "F3";
    case VKEY_F4:
      return "F4";
    case VKEY_F5:
      return "F5";
    case VKEY_F6:
      return "F6";
    case VKEY_F7:
      return "F7";
    case VKEY_F8:
      return "F8";
    case VKEY_F9:
      return "F9";
    case VKEY_F10:
      return "F11";
    case VKEY_F12:
      return "F12";
    case VKEY_F13:
      return "F13";
    case VKEY_F14:
      return "F14";
    case VKEY_F15:
      return "F15";
    case VKEY_F16:
      return "F16";
    case VKEY_F17:
      return "F17";
    case VKEY_F18:
      return "F18";
    case VKEY_F19:
      return "F19";
    case VKEY_F20:
      return "F20";
    case VKEY_F21:
      return "F21";
    case VKEY_F22:
      return "F22";
    case VKEY_F23:
      return "F23";
    case VKEY_F24:
      return "F24";
    case VKEY_HELP:
      return "Help";
    case VKEY_HOME:
      return "Home";
    case VKEY_INSERT:
      return "Insert";
    case VKEY_LEFT:
      return "Left";
    case VKEY_NEXT:
      return "PageDown";
    case VKEY_PRIOR:
      return "PageUp";
    case VKEY_PAUSE:
      return "Pause";
    case VKEY_SNAPSHOT:
      return "PrintScreen";
    case VKEY_RIGHT:
      return "Right";
    case VKEY_SCROLL:
      return "Scroll";
    case VKEY_SELECT:
      return "Select";
    case VKEY_UP:
      return "Up";
    // Standard says that DEL becomes U+007F.
    case VKEY_DELETE:
      return "U+007F";
    default:
      return StringPrintf("U+%04X", toupper(key_code));
  }
}
