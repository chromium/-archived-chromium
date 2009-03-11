// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/text_input_controller.h"

#include "testing/gtest/include/gtest/gtest.h"

// Test class to let us check TextInputController's method table.
class TestTextInputController : public TextInputController {
 public:
  TestTextInputController() : TextInputController(NULL) {}

  size_t MethodCount() {
    return methods_.size();
  }
};


TEST(TextInputControllerTest, MethodMapIsInitialized) {
  TestTextInputController text_input_controller;

  EXPECT_EQ(14U, text_input_controller.MethodCount());

  EXPECT_TRUE(text_input_controller.IsMethodRegistered(
      "insertText"));
  EXPECT_TRUE(text_input_controller.IsMethodRegistered(
      "doCommand"));
  EXPECT_TRUE(text_input_controller.IsMethodRegistered(
      "setMarkedText"));
  EXPECT_TRUE(text_input_controller.IsMethodRegistered(
      "unmarkText"));
  EXPECT_TRUE(text_input_controller.IsMethodRegistered(
      "hasMarkedText"));
  EXPECT_TRUE(text_input_controller.IsMethodRegistered(
      "conversationIdentifier"));
  EXPECT_TRUE(text_input_controller.IsMethodRegistered(
      "substringFromRange"));
  EXPECT_TRUE(text_input_controller.IsMethodRegistered(
      "attributedSubstringFromRange"));
  EXPECT_TRUE(text_input_controller.IsMethodRegistered(
      "markedRange"));
  EXPECT_TRUE(text_input_controller.IsMethodRegistered(
      "selectedRange"));
  EXPECT_TRUE(text_input_controller.IsMethodRegistered(
      "firstRectForCharacterRange"));
  EXPECT_TRUE(text_input_controller.IsMethodRegistered(
      "characterIndexForPoint"));
  EXPECT_TRUE(text_input_controller.IsMethodRegistered(
      "validAttributesForMarkedText"));
  EXPECT_TRUE(text_input_controller.IsMethodRegistered(
      "makeAttributedString"));

  // Negative test.
  EXPECT_FALSE(text_input_controller.IsMethodRegistered(
      "momeRathsOutgrabe"));
}
