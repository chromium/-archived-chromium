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
// Class WebTextInputImpl provides implementation of WebTextInput which is
// used by TextInputController in test_shell. It only facilitates layout tests
// and should not be used by renderers.

#ifndef WEBKIT_GLUE_WEBTEXTINPUT_IMPL_H__
#define WEBKIT_GLUE_WEBTEXTINPUT_IMPL_H__

#include "webkit/glue/webtextinput.h"

class WebFrameImpl;
class WebCore::Editor;

class WebTextInputImpl : public WebTextInput {
 public:
  WebTextInputImpl(WebFrameImpl* web_frame_impl);
  virtual ~WebTextInputImpl();

  // WebTextInput methods
  virtual void InsertText(std::string& text);
  virtual void DoCommand(std::string& command);
  virtual void SetMarkedText(std::string& text,
                             int32_t location, int32_t length);
  virtual void UnMarkText();
  virtual bool HasMarkedText();
  virtual void MarkedRange(std::string* range_str);
  virtual void SelectedRange(std::string* range_str);
  virtual void ValidAttributesForMarkedText(std::string* attributes);

  // TODO(huanr): examine all layout tests involving TextInputController
  // and implement these functions if necessary.
  virtual void ConversationIdentifier();
  virtual void SubstringFromRange(int32_t location, int32_t length);
  virtual void AttributedSubstringFromRange(int32_t location,
                                            int32_t length);
  virtual void FirstRectForCharacterRange(int32_t location,
                                          int32_t length);
  virtual void CharacterIndexForPoint(double x, double y);
  virtual void MakeAttributedString(std::string& str);

 private:
  WebCore::Frame* GetFrame();
  WebCore::Editor* GetEditor();

  void DeleteToEndOfParagraph();

  // Holding a non-owning pointer to the web frame we are associated with.
  WebFrameImpl* web_frame_impl_;

  DISALLOW_EVIL_CONSTRUCTORS(WebTextInputImpl);
};

#endif  // #ifndef WEBKIT_GLUE_WEBTEXTINPUT_IMPL_H__
