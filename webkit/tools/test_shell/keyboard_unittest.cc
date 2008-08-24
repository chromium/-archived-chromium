// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#pragma warning(push, 0)
#include "EventNames.h"
#include "EventTarget.h"
#include "KeyboardEvent.h"
#pragma warning(pop)

#include "webkit/glue/editor_client_impl.h"
#include "webkit/glue/event_conversion.h"
#include "webkit/glue/webinputevent.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(KeyboardUnitTestKeyDown, TestCtrlReturn) {
  WebCore::EventNames::init();

  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = 0xD;
  keyboard_event.key_data = 0xD;
  keyboard_event.modifiers = WebInputEvent::CTRL_KEY;
  keyboard_event.type = WebInputEvent::KEY_DOWN;
  
  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::RawKeyDown);
  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);
  EXPECT_STREQ(result, "InsertNewline");
}

TEST(KeyboardUnitTestKeyDown, TestCtrlZ) {
  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = 'Z';
  keyboard_event.key_data = 'Z';
  keyboard_event.modifiers = WebInputEvent::CTRL_KEY;
  keyboard_event.type = WebInputEvent::KEY_DOWN;
  
  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::RawKeyDown);
  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);
  EXPECT_STREQ(result, "Undo");
}

TEST(KeyboardUnitTestKeyDown, TestCtrlA) {
  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = 'A';
  keyboard_event.key_data = 'A';
  keyboard_event.modifiers = WebInputEvent::CTRL_KEY;
  keyboard_event.type = WebInputEvent::KEY_DOWN;
  
  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::RawKeyDown);
  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);
  EXPECT_STREQ(result, "SelectAll");
}

TEST(KeyboardUnitTestKeyDown, TestCtrlX) {
  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = 'X';
  keyboard_event.key_data = 'X';
  keyboard_event.modifiers = WebInputEvent::CTRL_KEY;
  keyboard_event.type = WebInputEvent::KEY_DOWN;
  
  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::RawKeyDown);
  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);

  EXPECT_STREQ(result, "Cut");
}

TEST(KeyboardUnitTestKeyDown, TestCtrlC) {
  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = 'C';
  keyboard_event.key_data = 'C';
  keyboard_event.modifiers = WebInputEvent::CTRL_KEY;
  keyboard_event.type = WebInputEvent::KEY_DOWN;
  
  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::RawKeyDown);
  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);
  EXPECT_STREQ(result, "Copy");
}

TEST(KeyboardUnitTestKeyDown, TestCtrlV) {
  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = 'V';
  keyboard_event.key_data = 'V';
  keyboard_event.modifiers = WebInputEvent::CTRL_KEY;
  keyboard_event.type = WebInputEvent::KEY_DOWN;
  
  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::RawKeyDown);
  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);
  EXPECT_STREQ(result, "Paste");
}

TEST(KeyboardUnitTestKeyDown, TestEscape) {
  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = VK_ESCAPE;
  keyboard_event.key_data = VK_ESCAPE;
  keyboard_event.modifiers = 0;
  keyboard_event.type = WebInputEvent::KEY_DOWN;
  
  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::RawKeyDown);
  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);
  EXPECT_STREQ(result, "Cancel");
}

TEST(KeyboardUnitTestKeyDown, TestRedo) {
  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = 'Y';
  keyboard_event.key_data = 'Y';
  keyboard_event.modifiers = WebInputEvent::CTRL_KEY;
  keyboard_event.type = WebInputEvent::KEY_DOWN;
  
  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::RawKeyDown);
  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);
  EXPECT_STREQ(result, "Redo");
}


TEST(KeyboardUnitTestKeyPress, TestInsertTab) {
  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = '\t';
  keyboard_event.key_data = '\t';
  keyboard_event.modifiers = 0;
  keyboard_event.type = WebInputEvent::KEY_DOWN;

  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::Char); 

  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);
  EXPECT_STREQ(result, "InsertTab");
}

TEST(KeyboardUnitTestKeyPress, TestInsertBackTab) {
  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = '\t';
  keyboard_event.key_data = '\t';
  keyboard_event.modifiers = WebInputEvent::SHIFT_KEY;
  keyboard_event.type = WebInputEvent::KEY_DOWN;

  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::Char); 

  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);
  EXPECT_STREQ(result, "InsertBacktab");
}

TEST(KeyboardUnitTestKeyPress, TestInsertNewline) {
  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = '\r';
  keyboard_event.key_data = '\r';
  keyboard_event.modifiers = 0;
  keyboard_event.type = WebInputEvent::KEY_DOWN;

  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::Char); 

  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);
  EXPECT_STREQ(result, "InsertNewline");
}

TEST(KeyboardUnitTestKeyPress, TestInsertNewline2) {
  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = '\r';
  keyboard_event.key_data = '\r';
  keyboard_event.modifiers = WebInputEvent::CTRL_KEY;
  keyboard_event.type = WebInputEvent::KEY_DOWN;

  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::Char); 

  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);
  EXPECT_STREQ(result, "InsertNewline");
}

TEST(KeyboardUnitTestKeyPress, TestInsertlinebreak) {
  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = '\r';
  keyboard_event.key_data = '\r';
  keyboard_event.modifiers = WebInputEvent::SHIFT_KEY;
  keyboard_event.type = WebInputEvent::KEY_DOWN;

  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::Char); 

  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);
  EXPECT_STREQ(result, "InsertLineBreak");
}

TEST(KeyboardUnitTestKeyPress, TestInsertNewline3) {
  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = '\r';
  keyboard_event.key_data = '\r';
  keyboard_event.modifiers = WebInputEvent::ALT_KEY;
  keyboard_event.type = WebInputEvent::KEY_DOWN;

  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::Char); 

  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);
  EXPECT_STREQ(result, "InsertNewline");
}

TEST(KeyboardUnitTestKeyPress, TestInsertNewline4) {
  EditorClientImpl editor_impl(NULL);

  WebKeyboardEvent keyboard_event;
  keyboard_event.key_code = '\r';
  keyboard_event.key_data = '\r';
  keyboard_event.modifiers = WebInputEvent::ALT_KEY | WebInputEvent::SHIFT_KEY;
  keyboard_event.type = WebInputEvent::KEY_DOWN;

  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(WebCore::PlatformKeyboardEvent::Char); 

  WebCore::KeyboardEvent keyboardEvent(evt, NULL);
  const char* result = editor_impl.interpretKeyEvent(&keyboardEvent);
  EXPECT_STREQ(result, "InsertNewline");
}
