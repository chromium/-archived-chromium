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

// TextInputController is bound to window.textInputController in Javascript
// when test_shell is running noninteractively.  Layout tests use it to
// exercise various corners of text input.
//
// Mac equivalent: WebKit/WebKitTools/DumpRenderTree/TextInputController.{h,m}

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEXT_INPUT_CONTROLLER_H__
#define WEBKIT_TOOLS_TEST_SHELL_TEXT_INPUT_CONTROLLER_H__

#include "webkit/glue/cpp_bound_class.h"

class TestShell;
class WebView;
class WebTextInput;

class TextInputController : public CppBoundClass {
 public:
  TextInputController(TestShell* shell);

  void insertText(const CppArgumentList& args, CppVariant* result);
  void doCommand(const CppArgumentList& args, CppVariant* result);
  void setMarkedText(const CppArgumentList& args, CppVariant* result);
  void unmarkText(const CppArgumentList& args, CppVariant* result);
  void hasMarkedText(const CppArgumentList& args, CppVariant* result);
  void conversationIdentifier(const CppArgumentList& args, CppVariant* result);
  void substringFromRange(const CppArgumentList& args, CppVariant* result);
  void attributedSubstringFromRange(const CppArgumentList& args, CppVariant* result);
  void markedRange(const CppArgumentList& args, CppVariant* result);
  void selectedRange(const CppArgumentList& args, CppVariant* result);
  void firstRectForCharacterRange(const CppArgumentList& args, CppVariant* result);
  void characterIndexForPoint(const CppArgumentList& args, CppVariant* result);
  void validAttributesForMarkedText(const CppArgumentList& args, CppVariant* result);
  void makeAttributedString(const CppArgumentList& args, CppVariant* result);

 private:
  static WebTextInput* GetTextInput();

  // Returns the test shell's webview.
  static WebView* webview();

  // Non-owning pointer.  The LayoutTestController is owned by the host.
  static TestShell* shell_;
};

#endif // WEBKIT_TOOLS_TEST_SHELL_TEXT_INPUT_CONTROLLER_H__
