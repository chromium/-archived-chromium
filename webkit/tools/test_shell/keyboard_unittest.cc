// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "EventTarget.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
MSVC_POP_WARNING();

#undef LOG

#include "base/string_util.h"
#include "webkit/glue/editor_client_impl.h"
#include "webkit/glue/event_conversion.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"

using WebCore::PlatformKeyboardEvent;
using WebCore::KeyboardEvent;

using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;

class KeyboardTest : public testing::Test {
 public:
  void SetUp() {
    WTF::initializeThreading();
  }

  // Pass a WebKeyboardEvent into the EditorClient and get back the string
  // name of which editing event that key causes.
  // E.g., sending in the enter key gives back "InsertNewline".
  const char* InterpretKeyEvent(
      const WebKeyboardEvent& keyboard_event,
      PlatformKeyboardEvent::Type key_type) {
    EditorClientImpl editor_impl(NULL);

    MakePlatformKeyboardEvent evt(keyboard_event);
    evt.SetKeyType(key_type);
    RefPtr<KeyboardEvent> keyboardEvent = KeyboardEvent::create(evt, NULL);
    return editor_impl.interpretKeyEvent(keyboardEvent.get());
  }

  // Set up a WebKeyboardEvent KEY_DOWN event with key code and modifiers.
  void SetupKeyDownEvent(WebKeyboardEvent* keyboard_event,
                         char key_code,
                         int modifiers) {
    keyboard_event->windowsKeyCode = key_code;
    keyboard_event->modifiers = modifiers;
    keyboard_event->type = WebInputEvent::KeyDown;
    keyboard_event->text[0] = key_code;
    keyboard_event->setKeyIdentifierFromWindowsKeyCode();
  }

  // Like InterpretKeyEvent, but with pressing down OSModifier+|key_code|.
  // OSModifier is the platform's standard modifier key: control on most
  // platforms, but meta (command) on Mac.
  const char* InterpretOSModifierKeyPress(char key_code) {
    WebKeyboardEvent keyboard_event;
#if defined(OS_WIN) || defined(OS_LINUX)
    WebInputEvent::Modifiers os_modifier = WebInputEvent::ControlKey;
#elif defined(OS_MACOSX)
    WebInputEvent::Modifiers os_modifier = WebInputEvent::MetaKey;
#endif
    SetupKeyDownEvent(&keyboard_event, key_code, os_modifier);
    return InterpretKeyEvent(keyboard_event, PlatformKeyboardEvent::RawKeyDown);
  }

  // Like InterpretKeyEvent, but with pressing down ctrl+|key_code|.
  const char* InterpretCtrlKeyPress(char key_code) {
    WebKeyboardEvent keyboard_event;
    SetupKeyDownEvent(&keyboard_event, key_code, WebInputEvent::ControlKey);
    return InterpretKeyEvent(keyboard_event, PlatformKeyboardEvent::RawKeyDown);
  }

  // Like InterpretKeyEvent, but with typing a tab.
  const char* InterpretTab(int modifiers) {
    WebKeyboardEvent keyboard_event;
    SetupKeyDownEvent(&keyboard_event, '\t', modifiers);
    return InterpretKeyEvent(keyboard_event, PlatformKeyboardEvent::Char);
  }

  // Like InterpretKeyEvent, but with typing a newline.
  const char* InterpretNewLine(int modifiers) {
    WebKeyboardEvent keyboard_event;
    SetupKeyDownEvent(&keyboard_event, '\r', modifiers);
    return InterpretKeyEvent(keyboard_event, PlatformKeyboardEvent::Char);
  }

  // A name for "no modifiers set".
  static const int kNoModifiers = 0;
};

TEST_F(KeyboardTest, TestCtrlReturn) {
  EXPECT_STREQ("InsertNewline", InterpretCtrlKeyPress(0xD));
}

TEST_F(KeyboardTest, TestOSModifierZ) {
  EXPECT_STREQ("Undo", InterpretOSModifierKeyPress('Z'));
}

TEST_F(KeyboardTest, TestOSModifierY) {
  EXPECT_STREQ("Redo", InterpretOSModifierKeyPress('Y'));
}

TEST_F(KeyboardTest, TestOSModifierA) {
  EXPECT_STREQ("SelectAll", InterpretOSModifierKeyPress('A'));
}

TEST_F(KeyboardTest, TestOSModifierX) {
  EXPECT_STREQ("Cut", InterpretOSModifierKeyPress('X'));
}

TEST_F(KeyboardTest, TestOSModifierC) {
  EXPECT_STREQ("Copy", InterpretOSModifierKeyPress('C'));
}

TEST_F(KeyboardTest, TestOSModifierV) {
  EXPECT_STREQ("Paste", InterpretOSModifierKeyPress('V'));
}

TEST_F(KeyboardTest, TestEscape) {
  WebKeyboardEvent keyboard_event;
  SetupKeyDownEvent(&keyboard_event, WebCore::VKEY_ESCAPE, kNoModifiers);

  const char* result = InterpretKeyEvent(keyboard_event,
                                         PlatformKeyboardEvent::RawKeyDown);
  EXPECT_STREQ("Cancel", result);
}

TEST_F(KeyboardTest, TestInsertTab) {
  EXPECT_STREQ("InsertTab", InterpretTab(kNoModifiers));
}

TEST_F(KeyboardTest, TestInsertBackTab) {
  EXPECT_STREQ("InsertBacktab", InterpretTab(WebInputEvent::ShiftKey));
}

TEST_F(KeyboardTest, TestInsertNewline) {
  EXPECT_STREQ("InsertNewline", InterpretNewLine(kNoModifiers));
}

TEST_F(KeyboardTest, TestInsertNewline2) {
  EXPECT_STREQ("InsertNewline", InterpretNewLine(WebInputEvent::ControlKey));
}

TEST_F(KeyboardTest, TestInsertLineBreak) {
  EXPECT_STREQ("InsertLineBreak", InterpretNewLine(WebInputEvent::ShiftKey));
}

TEST_F(KeyboardTest, TestInsertNewline3) {
  EXPECT_STREQ("InsertNewline", InterpretNewLine(WebInputEvent::AltKey));
}

TEST_F(KeyboardTest, TestInsertNewline4) {
  int modifiers = WebInputEvent::AltKey | WebInputEvent::ShiftKey;
  const char* result = InterpretNewLine(modifiers);
  EXPECT_STREQ("InsertNewline", result);
}
