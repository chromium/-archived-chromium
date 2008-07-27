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
  virtual void InsertText(std::string& text) = 0;

  // Executes the given editing command on the frame.
  virtual void DoCommand(std::string& command) = 0;

  // Sets marked text region on the frame.
  virtual void SetMarkedText(std::string& text,
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
  virtual void MakeAttributedString(std::string& str) = 0;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WebTextInput);
};

#endif  // #ifndef WEBKIT_GLUE_WEBTEXTINPUT_H__
