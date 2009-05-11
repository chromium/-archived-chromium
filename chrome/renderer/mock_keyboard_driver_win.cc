// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/mock_keyboard_driver_win.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/renderer/mock_keyboard.h"

MockKeyboardDriverWin::MockKeyboardDriverWin() {
  // Save the keyboard layout and status of the application.
  // This class changes the keyboard layout and status of this application.
  // This change may break succeeding tests. To prevent this possible break, we
  // should save the layout and status here to restore when this instance is
  // destroyed.
  original_keyboard_layout_ = GetKeyboardLayout(0);
  GetKeyboardState(&original_keyboard_states_[0]);

  keyboard_handle_ = NULL;
  memset(&keyboard_states_[0], 0, sizeof(keyboard_states_));
}

MockKeyboardDriverWin::~MockKeyboardDriverWin() {
  // Unload the keyboard-layout driver, restore the keyboard state, and reset
  // the keyboard layout for succeeding tests.
  if (keyboard_handle_)
    UnloadKeyboardLayout(keyboard_handle_);
  SetKeyboardState(&original_keyboard_states_[0]);
  ActivateKeyboardLayout(original_keyboard_layout_, KLF_RESET);
}

bool MockKeyboardDriverWin::SetLayout(int layout) {
  // Unload the current keyboard-layout driver and load a new keyboard-layout
  // driver for mapping a virtual key-code to a Unicode character.
  if (keyboard_handle_) {
    UnloadKeyboardLayout(keyboard_handle_);
    keyboard_handle_ = NULL;
  }

  // Scan the mapping table and retrieve a Language ID for the input layout.
  // Load the keyboard-layout driver when we find a Language ID.
  // This Language IDs are copied from the registry
  //   "HKLM\SYSTEM\CurrentControlSet\Control\Keyboard layouts".
  // TODO(hbono): Add more keyboard-layout drivers.
  static const struct {
    const wchar_t* language;
    MockKeyboard::Layout keyboard_layout;
  } kLanguageIDs[] = {
    {L"00000401", MockKeyboard::LAYOUT_ARABIC},
    {L"00000402", MockKeyboard::LAYOUT_BULGARIAN},
    {L"00000404", MockKeyboard::LAYOUT_CHINESE_TRADITIONAL},
    {L"00000405", MockKeyboard::LAYOUT_CZECH},
    {L"00000406", MockKeyboard::LAYOUT_DANISH},
    {L"00000407", MockKeyboard::LAYOUT_GERMAN},
    {L"00000408", MockKeyboard::LAYOUT_GREEK},
    {L"00000409", MockKeyboard::LAYOUT_UNITED_STATES},
    {L"0000040a", MockKeyboard::LAYOUT_SPANISH},
    {L"0000040b", MockKeyboard::LAYOUT_FINNISH},
    {L"0000040c", MockKeyboard::LAYOUT_FRENCH},
    {L"0000040d", MockKeyboard::LAYOUT_HEBREW},
    {L"0000040e", MockKeyboard::LAYOUT_HUNGARIAN},
    {L"00000410", MockKeyboard::LAYOUT_ITALIAN},
    {L"00000411", MockKeyboard::LAYOUT_JAPANESE},
    {L"00000412", MockKeyboard::LAYOUT_KOREAN},
    {L"00000415", MockKeyboard::LAYOUT_POLISH},
    {L"00000416", MockKeyboard::LAYOUT_PORTUGUESE_BRAZILIAN},
    {L"00000418", MockKeyboard::LAYOUT_ROMANIAN},
    {L"00000419", MockKeyboard::LAYOUT_RUSSIAN},
    {L"0000041a", MockKeyboard::LAYOUT_CROATIAN},
    {L"0000041b", MockKeyboard::LAYOUT_SLOVAK},
    {L"0000041e", MockKeyboard::LAYOUT_THAI},
    {L"0000041d", MockKeyboard::LAYOUT_SWEDISH},
    {L"0000041f", MockKeyboard::LAYOUT_TURKISH_Q},
    {L"0000042a", MockKeyboard::LAYOUT_VIETNAMESE},
    {L"00000439", MockKeyboard::LAYOUT_DEVANAGARI_INSCRIPT},
    {L"00000816", MockKeyboard::LAYOUT_PORTUGUESE},
    {L"00001409", MockKeyboard::LAYOUT_UNITED_STATES_DVORAK},
    {L"00001009", MockKeyboard::LAYOUT_CANADIAN_FRENCH},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kLanguageIDs); ++i) {
    if (layout == kLanguageIDs[i].keyboard_layout) {
      keyboard_handle_ = LoadKeyboardLayout(kLanguageIDs[i].language,
                                            KLF_ACTIVATE);
      if (!keyboard_handle_)
        return false;
      return true;
    }
  }

  // Return false if there are not any matching drivers.
  return false;
}

bool MockKeyboardDriverWin::SetModifiers(int modifiers) {
  // Over-write the keyboard status with our modifier-key status.
  // WebInputEventFactory::keyboardEvent() uses GetKeyState() to retrive
  // modifier-key status. So, we update the modifier-key status with this
  // SetKeyboardState() call before creating NativeWebKeyboardEvent
  // instances.
  memset(&keyboard_states_[0], 0, sizeof(keyboard_states_));
  static const struct {
    int key_code;
    int mask;
  } kModifierMasks[] = {
    {VK_SHIFT,    MockKeyboard::LEFT_SHIFT | MockKeyboard::RIGHT_SHIFT},
    {VK_CONTROL,  MockKeyboard::LEFT_CONTROL | MockKeyboard::RIGHT_CONTROL},
    {VK_MENU,     MockKeyboard::LEFT_ALT | MockKeyboard::RIGHT_ALT},
    {VK_LSHIFT,   MockKeyboard::LEFT_SHIFT},
    {VK_LCONTROL, MockKeyboard::LEFT_CONTROL},
    {VK_LMENU,    MockKeyboard::LEFT_ALT},
    {VK_RSHIFT,   MockKeyboard::RIGHT_SHIFT},
    {VK_RCONTROL, MockKeyboard::RIGHT_CONTROL},
    {VK_RMENU,    MockKeyboard::RIGHT_ALT},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kModifierMasks); ++i) {
    const int kKeyDownMask = 0x80;
    if (modifiers & kModifierMasks[i].mask)
      keyboard_states_[kModifierMasks[i].key_code] = kKeyDownMask;
  }
  SetKeyboardState(&keyboard_states_[0]);

  return true;
}

int MockKeyboardDriverWin::GetCharacters(int key_code,
                                         std::wstring* output) {
  // Retrieve Unicode characters composed from the input key-code and
  // the mofifiers.
  CHECK(output);
  wchar_t code[16];
  int length = ToUnicodeEx(key_code, MapVirtualKey(key_code, 0),
                           &keyboard_states_[0], &code[0],
                           ARRAYSIZE_UNSAFE(code), 0,
                           keyboard_handle_);
  if (length > 0)
    output->assign(code);
  return length;
}
