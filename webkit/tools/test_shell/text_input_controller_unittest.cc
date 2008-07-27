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
  
  EXPECT_EQ(14, text_input_controller.MethodCount());
  
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
