// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TextInputController is bound to window.textInputController in Javascript
// when test_shell is running in layout test mode.  Layout tests use it to
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
