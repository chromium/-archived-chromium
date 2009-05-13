// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/text_input_controller.h"

#include "webkit/glue/webview.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webtextinput.h"
#include "webkit/tools/test_shell/test_shell.h"

TestShell* TextInputController::shell_ = NULL;

TextInputController::TextInputController(TestShell* shell) {
  // Set static shell_ variable. Be careful not to assign shell_ to new
  // windows which are temporary.
  if (NULL == shell_)
    shell_ = shell;

  BindMethod("insertText", &TextInputController::insertText);
  BindMethod("doCommand", &TextInputController::doCommand);
  BindMethod("setMarkedText", &TextInputController::setMarkedText);
  BindMethod("unmarkText", &TextInputController::unmarkText);
  BindMethod("hasMarkedText", &TextInputController::hasMarkedText);
  BindMethod("conversationIdentifier", &TextInputController::conversationIdentifier);
  BindMethod("substringFromRange", &TextInputController::substringFromRange);
  BindMethod("attributedSubstringFromRange", &TextInputController::attributedSubstringFromRange);
  BindMethod("markedRange", &TextInputController::markedRange);
  BindMethod("selectedRange", &TextInputController::selectedRange);
  BindMethod("firstRectForCharacterRange", &TextInputController::firstRectForCharacterRange);
  BindMethod("characterIndexForPoint", &TextInputController::characterIndexForPoint);
  BindMethod("validAttributesForMarkedText", &TextInputController::validAttributesForMarkedText);
  BindMethod("makeAttributedString", &TextInputController::makeAttributedString);
}

/* static */ WebView* TextInputController::webview() {
  return shell_->webView();
}

/* static */ WebTextInput* TextInputController::GetTextInput() {
  return webview()->GetMainFrame()->GetTextInput();
}

void TextInputController::insertText(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebTextInput* text_input = GetTextInput();
  if (!text_input)
    return;

  if (args.size() >= 1 && args[0].isString()) {
    text_input->InsertText(args[0].ToString());
  }
}

void TextInputController::doCommand(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebTextInput* text_input = GetTextInput();
  if (!text_input)
    return;

  if (args.size() >= 1 && args[0].isString()) {
    text_input->DoCommand(args[0].ToString());
  }
}

void TextInputController::setMarkedText(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebTextInput* text_input = GetTextInput();
  if (!text_input)
    return;

  if (args.size() >= 3 && args[0].isString()
      && args[1].isNumber() && args[2].isNumber()) {
    text_input->SetMarkedText(args[0].ToString(),
                              args[1].ToInt32(),
                              args[2].ToInt32());
  }
}

void TextInputController::unmarkText(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebTextInput* text_input = GetTextInput();
  if (!text_input)
    return;

  text_input->UnMarkText();
}

void TextInputController::hasMarkedText(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebTextInput* text_input = GetTextInput();
  if (!text_input)
    return;

  result->Set(text_input->HasMarkedText());
}

void TextInputController::conversationIdentifier(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebTextInput* text_input = GetTextInput();
  if (!text_input)
    return;

  text_input->ConversationIdentifier();
}

void TextInputController::substringFromRange(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebTextInput* text_input = GetTextInput();
  if (!text_input)
    return;

  if (args.size() >= 2 && args[0].isNumber() && args[1].isNumber()) {
    text_input->SubstringFromRange(args[0].ToInt32(), args[1].ToInt32());
  }
}

void TextInputController::attributedSubstringFromRange(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebTextInput* text_input = GetTextInput();
  if (!text_input)
    return;

  if (args.size() >= 2 && args[0].isNumber() && args[1].isNumber()) {
    text_input->AttributedSubstringFromRange(args[0].ToInt32(),
                                             args[1].ToInt32());
  }
}

void TextInputController::markedRange(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebTextInput* text_input = GetTextInput();
  if (!text_input)
    return;

  std::string range_str;
  text_input->MarkedRange(&range_str);
  result->Set(range_str);
}

void TextInputController::selectedRange(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebTextInput* text_input = GetTextInput();
  if (!text_input)
    return;

  std::string range_str;
  text_input->SelectedRange(&range_str);
  result->Set(range_str);
}

void TextInputController::firstRectForCharacterRange(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebTextInput* text_input = GetTextInput();
  if (!text_input)
    return;

  if (args.size() >= 2 && args[0].isNumber() && args[1].isNumber()) {
    text_input->FirstRectForCharacterRange(args[0].ToInt32(),
                                           args[1].ToInt32());
  }
}

void TextInputController::characterIndexForPoint(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebTextInput* text_input = GetTextInput();
  if (!text_input)
    return;

  if (args.size() >= 2 && args[0].isDouble() && args[1].isDouble()) {
    text_input->CharacterIndexForPoint(args[0].ToDouble(),
                                       args[1].ToDouble());
  }
}

void TextInputController::validAttributesForMarkedText(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebTextInput* text_input = GetTextInput();
  if (!text_input)
    return;

  std::string attributes_str;
  text_input->ValidAttributesForMarkedText(&attributes_str);
  result->Set(attributes_str);
}

void TextInputController::makeAttributedString(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebTextInput* text_input = GetTextInput();
  if (!text_input)
    return;

  if (args.size() >= 1 && args[0].isString()) {
    text_input->MakeAttributedString(args[0].ToString());
  }
}
