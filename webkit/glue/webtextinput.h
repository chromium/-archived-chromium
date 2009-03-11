// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Class WebTextInput provides text input APIs used by TextInputController
// in test_shell. It only facilitates layout tests and should not be used
// by renderers.

#ifndef WEBKIT_GLUE_WEBTEXTINPUT_H__
#define WEBKIT_GLUE_WEBTEXTINPUT_H__

#include <string>
#include "base/basictypes.h"

class WebTextInput {
 public:
  WebTextInput() {}
  virtual ~WebTextInput() {}

  // Inserts text to the associated frame.
  virtual void InsertText(const std::string& text) = 0;

  // Executes the given editing command on the frame.
  virtual void DoCommand(const std::string& command) = 0;

  // Sets marked text region on the frame.
  virtual void SetMarkedText(const std::string& text,
                             int32_t location, int32_t length) = 0;

  // Clears the marked text region on the frame.
  virtual void UnMarkText() = 0;

  // Returns true if there are marked texts on the frame, returns false
  // otherwise.
  virtual bool HasMarkedText() = 0;

  // Writes the textual representation of the marked range on the frame to
  // range_str.
  virtual void MarkedRange(std::string* range_str) = 0;

  // Writes the textual representation of the selected range on the frame to
  // range_str.
  virtual void SelectedRange(std::string* range_str) = 0;

  // Writes the textual representation of the attributes of marked text range
  // on the frame to attributes.
  virtual void ValidAttributesForMarkedText(std::string* attributes) = 0;

  virtual void ConversationIdentifier() = 0;
  virtual void SubstringFromRange(int32_t location, int32_t length) = 0;
  virtual void AttributedSubstringFromRange(int32_t location,
                                            int32_t length) = 0;
  virtual void FirstRectForCharacterRange(int32_t location,
                                          int32_t length) = 0;
  virtual void CharacterIndexForPoint(double x, double y) = 0;
  virtual void MakeAttributedString(const std::string& str) = 0;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WebTextInput);
};

#endif  // #ifndef WEBKIT_GLUE_WEBTEXTINPUT_H__
