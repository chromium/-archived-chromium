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

#include <ctype.h>
#include "config.h"

#pragma warning(push, 0)
#include "Frame.h"
#include "Editor.h"
#pragma warning(pop)

#undef LOG

#include "base/string_util.h"
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

void WebTextInputImpl::InsertText(std::string& text) {
  WebCore::String str(text.c_str());
  GetEditor()->insertText(str, NULL);
}

void WebTextInputImpl::DoCommand(std::string& command) {
  // Since we don't have NSControl, we will convert the format of command
  // string and call the function on Editor directly.

  if (command.length() <= 2)
    return;

  // Make sure the first letter is upper case.
  command.replace(0, 1, 1, toupper(command.at(0)));

  // Remove the trailing ':' if existing.
  if (command.at(command.length() - 1) == ':')
    command.erase(command.length() - 1, 1);

  // Specially handling commands that Editor::execCommand does not directly
  // support.
  if (!command.compare("DeleteToEndOfParagraph")) {
    DeleteToEndOfParagraph();
  } else if(!command.compare("Indent")) {
    GetEditor()->indent();
  } else if(!command.compare("Outdent")) {
    GetEditor()->outdent();
  } else if(!command.compare("DeleteBackward")) {
    WebCore::AtomicString editor_command("BackwardDelete");
    GetEditor()->command(editor_command).execute();
  } else if(!command.compare("DeleteForward")) {
    WebCore::AtomicString editor_command("ForwardDelete");
    GetEditor()->command(editor_command).execute();
  } else {
    WebCore::AtomicString editor_command(command.c_str());
    GetEditor()->command(editor_command).execute();
  }

  return;
}

void WebTextInputImpl::SetMarkedText(std::string& text,
                                     int32_t location,
                                     int32_t length) {
  WebCore::Editor* editor = GetEditor();
  WebCore::String str(text.c_str());

  editor->confirmComposition(str);

  WebCore::Frame* frame = GetFrame();
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
  SStringPrintf(range_str, "%d,%d", range->startPosition().offset(),
                range->endPosition().offset());
}

void WebTextInputImpl::SelectedRange(std::string* range_str) {
  WTF::RefPtr<WebCore::Range> range
      = GetFrame()->selectionController()->toRange();

  // Range::toString() returns a string different from what test expects.
  // So we need to construct the string ourselves.
  SStringPrintf(range_str, "%d,%d", range.get()->startPosition().offset(),
                range.get()->endPosition().offset());
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

void WebTextInputImpl::MakeAttributedString(std::string& str) {
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
