// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "EventNames.h"
#include "EventTarget.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
MSVC_POP_WARNING();

#include "webkit/glue/editor_client_impl.h"
#include "webkit/glue/event_conversion.h"
#include "webkit/glue/webinputevent.h"

#include "testing/gtest/include/gtest/gtest.h"

using WebCore::PlatformKeyboardEvent;
using WebCore::KeyboardEvent;

static inline const char* InterpretKeyEvent(
	const WebKeyboardEvent& keyboard_event,
	PlatformKeyboardEvent::Type key_type) {
  EditorClientImpl editor_impl(NULL);

  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(key_type);
  RefPtr<KeyboardEvent> keyboardEvent = KeyboardEvent::create(evt, NULL);
  return editor_impl.interpretKeyEvent(keyboardEvent.get());
}

static inline void SetupKeyDownEvent(WebKeyboardEvent& keyboard_event,
                                     char key_code,
                                     int modifiers) {
  keyboard_event.key_code = key_code;
  keyboard_event.modifiers = modifiers;
  keyboard_event.type = WebInputEvent::KEY_DOWN;
#if defined(OS_LINUX)
  keyboard_event.text = key_code;
#endif
}

static inline const char* InterpretCtrlKeyPress(char key_code) {
  WebKeyboardEvent keyboard_event;
  SetupKeyDownEvent(keyboard_event, key_code, WebInputEvent::CTRL_KEY);

  return InterpretKeyEvent(keyboard_event, PlatformKeyboardEvent::RawKeyDown);
}

static const int no_modifiers = 0;

TEST(KeyboardUnitTestKeyDown, TestCtrlReturn) {
  // TODO(eseidel): This should be in a SETUP call using TEST_F
  WebCore::EventNames::init();

  EXPECT_STREQ("InsertNewline", InterpretCtrlKeyPress(0xD));
}

TEST(KeyboardUnitTestKeyDown, TestCtrlZ) {
  EXPECT_STREQ("Undo", InterpretCtrlKeyPress('Z'));
}

TEST(KeyboardUnitTestKeyDown, TestCtrlA) {
  EXPECT_STREQ("SelectAll", InterpretCtrlKeyPress('A'));
}

TEST(KeyboardUnitTestKeyDown, TestCtrlX) {
  EXPECT_STREQ("Cut", InterpretCtrlKeyPress('X'));
}

TEST(KeyboardUnitTestKeyDown, TestCtrlC) {
  EXPECT_STREQ("Copy", InterpretCtrlKeyPress('C'));
}

TEST(KeyboardUnitTestKeyDown, TestCtrlV) {
  EXPECT_STREQ("Paste", InterpretCtrlKeyPress('V'));
}

TEST(KeyboardUnitTestKeyDown, TestEscape) {
  WebKeyboardEvent keyboard_event;
  SetupKeyDownEvent(keyboard_event, WebCore::VKEY_ESCAPE, no_modifiers);

  const char* result = InterpretKeyEvent(keyboard_event,
                                         PlatformKeyboardEvent::RawKeyDown);
  EXPECT_STREQ("Cancel", result);
}

TEST(KeyboardUnitTestKeyDown, TestRedo) {
  EXPECT_STREQ(InterpretCtrlKeyPress('Y'), "Redo");
}

static inline const char* InterpretTab(int modifiers) {
  WebKeyboardEvent keyboard_event;
  SetupKeyDownEvent(keyboard_event, '\t', modifiers);
  return InterpretKeyEvent(keyboard_event, PlatformKeyboardEvent::Char);
}

TEST(KeyboardUnitTestKeyPress, TestInsertTab) {
  EXPECT_STREQ("InsertTab", InterpretTab(no_modifiers));
}

TEST(KeyboardUnitTestKeyPress, TestInsertBackTab) {
  EXPECT_STREQ("InsertBacktab", InterpretTab(WebInputEvent::SHIFT_KEY));
}

static inline const char* InterpretNewLine(int modifiers) {
  WebKeyboardEvent keyboard_event;
  SetupKeyDownEvent(keyboard_event, '\r', modifiers);
  return InterpretKeyEvent(keyboard_event, PlatformKeyboardEvent::Char);
}

TEST(KeyboardUnitTestKeyPress, TestInsertNewline) {
  EXPECT_STREQ("InsertNewline", InterpretNewLine(no_modifiers));
}

TEST(KeyboardUnitTestKeyPress, TestInsertNewline2) {
  EXPECT_STREQ("InsertNewline", InterpretNewLine(WebInputEvent::CTRL_KEY));
}

TEST(KeyboardUnitTestKeyPress, TestInsertlinebreak) {
  EXPECT_STREQ("InsertLineBreak", InterpretNewLine(WebInputEvent::SHIFT_KEY));
}

TEST(KeyboardUnitTestKeyPress, TestInsertNewline3) {
  EXPECT_STREQ("InsertNewline", InterpretNewLine(WebInputEvent::ALT_KEY));
}

TEST(KeyboardUnitTestKeyPress, TestInsertNewline4) {
  int modifiers = WebInputEvent::ALT_KEY | WebInputEvent::SHIFT_KEY;
  const char* result = InterpretNewLine(modifiers);
  EXPECT_STREQ(result, "InsertNewline");
}
