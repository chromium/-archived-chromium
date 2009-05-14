// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBTEXTINPUT_H_
#define WEBKIT_GLUE_WEBTEXTINPUT_H_

#include <string>
#include "base/basictypes.h"
#include "base/string16.h"

class WebTextInput {
 public:
  WebTextInput() {}
  virtual ~WebTextInput() {}

  // Inserts text to the associated frame.
  virtual void InsertText(const string16& text) = 0;

  // Executes the given editing command on the frame.
  virtual void DoCommand(const string16& command) = 0;

  // Sets marked text region on the frame.
  virtual void SetMarkedText(const string16& text,
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
  DISALLOW_COPY_AND_ASSIGN(WebTextInput);
};

#endif  // #ifndef WEBKIT_GLUE_WEBTEXTINPUT_H_
