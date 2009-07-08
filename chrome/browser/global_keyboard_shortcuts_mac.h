// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLOBAL_KEYBOARD_SHORTCUTS_MAC_H_
#define CHROME_BROWSER_GLOBAL_KEYBOARD_SHORTCUTS_MAC_H_

#include "base/basictypes.h";

struct KeyboardShortcutData {
  bool command_key;
  bool shift_key;
  bool cntrl_key;
  int vkey_code;  // Virtual Key code for the command.
  int chrome_command;  // The chrome command # to execute for this shortcut.
};

// Check if a given keycode + modifiers correspond to a given Chrome command.
// returns: Command number (as passed to Browser::ExecuteCommand) or -1 if there
// was no match.
int CommandForKeyboardShortcut(bool command_key, bool shift_key, bool cntrl_key,
    int vkey_code);

// For testing purposes.
const KeyboardShortcutData* GetKeyboardShortCutTable(size_t* num_entries);

#endif  // #ifndef CHROME_BROWSER_GLOBAL_KEYBOARD_SHORTCUTS_MAC_H_
