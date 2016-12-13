// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/global_keyboard_shortcuts_mac.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(GlobalKeyboardShortcuts, ShortCutsToCommand) {
  // Test that an invalid shortcut translates into an invalid command id.
  ASSERT_EQ(CommandForKeyboardShortcut(false, false, false, 0), -1);

  // Check that all known keyboard shortcuts return valid results.
  size_t num_shortcuts = 0;
  const KeyboardShortcutData *it = GetKeyboardShortCutTable(&num_shortcuts);
  ASSERT_GT(num_shortcuts, 0U);
  for (size_t i = 0; i < num_shortcuts; ++i, ++it) {
    int cmd_num = CommandForKeyboardShortcut(it->command_key, it->shift_key,
        it->cntrl_key, it->vkey_code);
    ASSERT_EQ(cmd_num, it->chrome_command);
  }
}
