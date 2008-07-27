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
// This file provides an abstract implementation of the inline autocomplete
// infrastructure defined in autocomplete_input_listener.h.

#include "webkit/glue/autocomplete_input_listener.h"

#pragma warning(push, 0)
#include "HTMLInputElement.h"
#include "HTMLFormElement.h"
#include "Document.h"
#include "Frame.h"
#include "Editor.h"
#include "EventNames.h"
#include "Event.h"
#pragma warning(pop)

#undef LOG

#include "base/logging.h"
#include "webkit/glue/editor_client_impl.h"
#include "webkit/glue/glue_util.h"

namespace webkit_glue {

// Hack (1 of 2) for http://bugs.webkit.org/show_bug.cgi?id=16976. This bug
// causes the caret position to be set after handling input events, which
// trumps our modifications, so for now we tell the EditorClient to preserve
// whatever selection set by our code.
// TODO(timsteele): Remove this function altogether once bug is fixed.
static void PreserveSelection(WebCore::HTMLInputElement* element) {
  WebCore::EditorClient* ec =
      element->form()->document()->frame()->editor()->client();
  EditorClientImpl* client = static_cast<EditorClientImpl*>(ec);
  client->PreserveSelection();
}

HTMLInputDelegate::HTMLInputDelegate(WebCore::HTMLInputElement* element)
    : element_(element) {
  // Reference the element for the lifetime of this delegate.
  // e is NULL when testing.
  if (element_)
    element_->ref();
}

HTMLInputDelegate::~HTMLInputDelegate() {
  if (element_)
    element_->deref();
}

bool HTMLInputDelegate::IsCaretAtEndOfText(size_t input_length,
                                           size_t previous_length) const {
  // Hack 2 of 2 for http://bugs.webkit.org/show_bug.cgi?id=16976.
  // TODO(timsteele): This check should only return early if
  // !(selectionEnd == selectionStart == user_input.length()).
  // However, because of webkit bug #16976 the caret is not properly moved
  // until after the handlers have executed, so for now we do the following
  // several checks. The first check handles the case webkit sets the End
  // selection but not the Start selection correctly, and the second is for
  // when webcore sets neither. This won't be perfect if the user moves the
  // selection around during inline autocomplete, but for now its the
  // friendliest behavior we can offer. Once the bug is fixed this method
  // should no longer need the previous_length parameter.
  if (((element_->selectionEnd() != element_->selectionStart() + 1) ||
       (element_->selectionEnd() != input_length)) &&
      ((element_->selectionEnd() != element_->selectionStart()) ||
       (element_->selectionEnd() != previous_length))) {
    return false;
  }
  return true;
}

void HTMLInputDelegate::SetValue(const std::wstring& value) {
  element_->setValue(StdWStringToString(value));
}

std::wstring HTMLInputDelegate::GetValue() const {
  return StringToStdWString(element_->value());
}

void HTMLInputDelegate::SetSelectionRange(size_t start, size_t end) {
  element_->setSelectionRange(start, end);
  // Hack, see comments for PreserveSelection().
  PreserveSelection(element_);
}

void HTMLInputDelegate::OnFinishedAutocompleting() {
  // This sets the input element to an autofilled state which will result in it
  // having a yellow background.
  element_->setAutofilled(true);
  // Notify any changeEvent listeners.
  element_->onChange();
}

AutocompleteInputListener::AutocompleteInputListener(
    AutocompleteEditDelegate* edit_delegate)
    : edit_delegate_(edit_delegate) {
  previous_text_ = edit_delegate->GetValue();
}
// The following method is based on Firefox2 code in
//  toolkit/components/autocomplete/src/nsAutoCompleteController.cpp
// Its license block is
//
 /* ***** BEGIN LICENSE BLOCK *****
  * Version: MPL 1.1/GPL 2.0/LGPL 2.1
  *
  * The contents of this file are subject to the Mozilla Public License Version
  * 1.1 (the "License"); you may not use this file except in compliance with
  * the License. You may obtain a copy of the License at
  * http://www.mozilla.org/MPL/
  *
  * Software distributed under the License is distributed on an "AS IS" basis,
  * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
  * for the specific language governing rights and limitations under the
  * License.
  *
  * The Original Code is Mozilla Communicator client code.
  *
  * The Initial Developer of the Original Code is
  * Netscape Communications Corporation.
  * Portions created by the Initial Developer are Copyright (C) 1998
  * the Initial Developer. All Rights Reserved.
  *
  * Contributor(s):
  *   Joe Hewitt <hewitt@netscape.com> (Original Author)
  *   Dean Tessman <dean_tessman@hotmail.com>
  *   Johnny Stenback <jst@mozilla.jstenback.com>
  *   Masayuki Nakano <masayuki@d-toybox.com>
  *
  * Alternatively, the contents of this file may be used under the terms of
  * either the GNU General Public License Version 2 or later (the "GPL"), or
  * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
  * in which case the provisions of the GPL or the LGPL are applicable instead
  * of those above. If you wish to allow use of your version of this file only
  * under the terms of either the GPL or the LGPL, and not to allow others to
  * use your version of this file under the terms of the MPL, indicate your
  * decision by deleting the provisions above and replace them with the notice
  * and other provisions required by the GPL or the LGPL. If you do not delete
  * the provisions above, a recipient may use your version of this file under
  * the terms of any one of the MPL, the GPL or the LGPL.
  *
  * ***** END LICENSE BLOCK ***** */
bool AutocompleteInputListener::ShouldInlineAutocomplete(
    const std::wstring& user_input) {
  size_t prev_length = previous_text_.length();
  // The following are a bunch of early returns in cases we don't want to
  // go through with inline autocomplete.

  // Don't bother doing AC if nothing changed.
  if (user_input.length() > 0 && (user_input == previous_text_))
    return false;

  // Did user backspace?
  if ((user_input.length() < previous_text_.length()) &&
       previous_text_.substr(0, user_input.length()) == user_input) {
    previous_text_ = user_input;
    return false;
  }

  // Remember the current input.
  previous_text_ = user_input;

  // Is search string empty?
  if (user_input.empty())
    return false;
  return edit_delegate_->IsCaretAtEndOfText(user_input.length(), prev_length);
}

void AutocompleteInputListener::handleEvent(WebCore::Event* event,
                                            bool /*is_window_event*/) {
  const WebCore::AtomicString& webcore_type = event->type();
  const std::wstring& user_input = edit_delegate_->GetValue();
  if (webcore_type == WebCore::EventNames::DOMFocusOutEvent) {
    OnBlur(user_input);
  } else if (webcore_type == WebCore::EventNames::inputEvent) {
    // Perform inline autocomplete if it is safe to do so.
    if (ShouldInlineAutocomplete(user_input))
      OnInlineAutocompleteNeeded(user_input);
  } else {
    NOTREACHED() << "unexpected EventName for autocomplete listener";
  }
}

void AttachForInlineAutocomplete(
    WebCore::HTMLInputElement* target,
    AutocompleteInputListener* listener) {
  target->addEventListener(WebCore::EventNames::DOMFocusOutEvent,
                           listener,
                           false);
  target->addEventListener(WebCore::EventNames::inputEvent,
                           listener,
                           false);
}

}  // webkit_glue
