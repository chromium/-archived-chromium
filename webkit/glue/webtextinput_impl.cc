// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctype.h>
#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "Frame.h"
#include "Editor.h"
MSVC_POP_WARNING();

#undef LOG

#include "base/string16.h"
#include "base/string_util.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webtextinput_impl.h"

WebTextInputImpl::WebTextInputImpl(WebFrameImpl* web_frame_impl)
    : WebTextInput(),
      web_frame_impl_(web_frame_impl) {
}

WebTextInputImpl::~WebTextInputImpl() {
}

WebCore::Frame* WebTextInputImpl::GetFrame() {
  return web_frame_impl_->frame();
}

WebCore::Editor* WebTextInputImpl::GetEditor() {
  return web_frame_impl_->frame()->editor();
}

void WebTextInputImpl::InsertText(const string16& text) {
  WebCore::String str = webkit_glue::String16ToString(text);
  GetEditor()->insertText(str, NULL);
}

void WebTextInputImpl::DoCommand(const string16& com) {
  if (com.length() <= 2)
    return;

  // Since we don't have NSControl, we will convert the format of command
  // string and call the function on Editor directly.
  string16 command = com;

  // Make sure the first letter is upper case.
  command.replace(0, 1, 1, toupper(command.at(0)));

  // Remove the trailing ':' if existing.
  if (command.at(command.length() - 1) == ':')
    command.erase(command.length() - 1, 1);

  // Specially handling commands that Editor::execCommand does not directly
  // support.
  if (EqualsASCII(command, "DeleteToEndOfParagraph")) {
    DeleteToEndOfParagraph();
  } else if(EqualsASCII(command, "Indent")) {
    GetEditor()->indent();
  } else if(EqualsASCII(command, "Outdent")) {
    GetEditor()->outdent();
  } else if(EqualsASCII(command, "DeleteBackward")) {
    WebCore::AtomicString editor_command("BackwardDelete");
    GetEditor()->command(editor_command).execute();
  } else if(EqualsASCII(command, "DeleteForward")) {
    WebCore::AtomicString editor_command("ForwardDelete");
    GetEditor()->command(editor_command).execute();
  } else {
    WebCore::AtomicString editor_command(command.c_str());
    GetEditor()->command(editor_command).execute();
  }

  return;
}

void WebTextInputImpl::SetMarkedText(const string16& text,
                                     int32_t location,
                                     int32_t length) {
  WebCore::Editor* editor = GetEditor();
  WebCore::String str = webkit_glue::String16ToString(text);

  editor->confirmComposition(str);

  WTF::Vector<WebCore::CompositionUnderline> decorations;

  editor->setComposition(str, decorations, location, length);
}

void WebTextInputImpl::UnMarkText() {
  GetEditor()->confirmCompositionWithoutDisturbingSelection();;
}

bool WebTextInputImpl::HasMarkedText() {
  return GetEditor()->hasComposition();
}

void WebTextInputImpl::ConversationIdentifier() {
}

void WebTextInputImpl::SubstringFromRange(int32_t location, int32_t length) {
}

void WebTextInputImpl::AttributedSubstringFromRange(int32_t location,
                                                    int32_t length) {
}

void WebTextInputImpl::MarkedRange(std::string* range_str) {
  RefPtr<WebCore::Range> range = GetEditor()->compositionRange();

  // Range::toString() returns a string different from what test expects.
  // So we need to construct the string ourselves.
  SStringPrintf(range_str, "%d,%d", range->startPosition().deprecatedEditingOffset(),
                range->endPosition().deprecatedEditingOffset());
}

void WebTextInputImpl::SelectedRange(std::string* range_str) {
  WTF::RefPtr<WebCore::Range> range
      = GetFrame()->selection()->toNormalizedRange();

  // Range::toString() returns a string different from what test expects.
  // So we need to construct the string ourselves.
  SStringPrintf(range_str, "%d,%d", range.get()->startPosition().deprecatedEditingOffset(),
                range.get()->endPosition().deprecatedEditingOffset());
}

void WebTextInputImpl::FirstRectForCharacterRange(int32_t location,
                                                  int32_t length) {
}

void WebTextInputImpl::CharacterIndexForPoint(double x, double y) {
}

void WebTextInputImpl::ValidAttributesForMarkedText(std::string* attributes) {
  // We simply return a string with relevant keywords.
  attributes->assign("NSUnderline,NSUnderlineColor,NSMarkedClauseSegment,NSTextInputReplacementRangeAttributeName");
}

void WebTextInputImpl::MakeAttributedString(const std::string& str) {
}

void WebTextInputImpl::DeleteToEndOfParagraph() {
  WebCore::Editor* editor = GetEditor();
  if (!editor->deleteWithDirection(WebCore::SelectionController::FORWARD,
                                   WebCore::ParagraphBoundary,
                                   true,
                                   false)) {
    editor->deleteWithDirection(WebCore::SelectionController::FORWARD,
                                WebCore::CharacterGranularity,
                                true,
                                false);
  }
}
