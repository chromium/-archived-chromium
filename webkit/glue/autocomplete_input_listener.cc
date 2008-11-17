// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides an abstract implementation of the inline autocomplete
// infrastructure defined in autocomplete_input_listener.h.

#include "webkit/glue/autocomplete_input_listener.h"
#include <set>

MSVC_PUSH_WARNING_LEVEL(0);
#include "HTMLInputElement.h"
#include "HTMLFormElement.h"
#include "Document.h"
#include "Frame.h"
#include "Editor.h"
#include "EventNames.h"
#include "Event.h"
#include "HTMLNames.h"
MSVC_POP_WARNING();

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

void HTMLInputDelegate::SetValue(const std::wstring& value) {
  element_->setValue(StdWStringToString(value));
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

AutocompleteBodyListener::AutocompleteBodyListener(WebCore::Frame* frame) {
  WebCore::HTMLElement* body = frame->document()->body();
  body->addEventListener(WebCore::eventNames().DOMFocusOutEvent, this, false);
  body->addEventListener(WebCore::eventNames().inputEvent, this, false);
  // Attaching to the WebCore body element effectively transfers ownership of
  // the listener object. When WebCore is tearing down the document, any
  // attached listeners are destroyed.
  // See Document::removeAllEventListenersFromAllNodes which is called by
  // FrameLoader::stopLoading. Also, there is no need for matching calls to
  // removeEventListener because the simplest and most convienient thing to do
  // for autocompletion is to stop listening once the element is destroyed.
}

AutocompleteBodyListener::~AutocompleteBodyListener() {
  // Delete the listener.  Pay special attention since we may have the same
  // listener registered for several elements.
  std::set<AutocompleteInputListener*> to_be_deleted_;
  for (InputElementInfoMap::iterator iter = elements_info_.begin();
       iter != elements_info_.end(); ++iter) {
    to_be_deleted_.insert(iter->second.listener);
  }

  std::set<AutocompleteInputListener*>::iterator iter;
  for (iter = to_be_deleted_.begin(); iter != to_be_deleted_.end(); ++iter)
    delete *iter;
  elements_info_.clear();
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
bool AutocompleteBodyListener::ShouldInlineAutocomplete(
    WebCore::HTMLInputElement* input,
    const std::wstring& old_text,
    const std::wstring& new_text) {
  size_t prev_length = old_text.length();
  // The following are a bunch of early returns in cases we don't want to
  // go through with inline autocomplete.

  // Don't bother doing AC if nothing changed.
  if (new_text.length() > 0 && (new_text == old_text))
    return false;

  // Did user backspace?
  if ((new_text.length() < old_text.length()) &&
       old_text.substr(0, new_text.length()) == new_text) {
    return false;
  }

  // Is search string empty?
  if (new_text.empty())
    return false;
  return IsCaretAtEndOfText(input, new_text.length(), prev_length);
}

void AutocompleteBodyListener::handleEvent(WebCore::Event* event,
                                           bool /*is_window_event*/) {
  const WebCore::AtomicString& webcore_type = event->type();
  DCHECK(event->target()->toNode());
  if (!event->target()->toNode()->hasTagName(WebCore::HTMLNames::inputTag))
    return;  // Not a node of interest to us.

  WebCore::HTMLInputElement* input =
      static_cast<WebCore::HTMLInputElement*>(event->target()->toNode());
  InputElementInfoMap::const_iterator iter = elements_info_.find(input);
  if (iter == elements_info_.end())
    return;  // Not an input node we are listening to.

  InputElementInfo input_info = iter->second;
  const std::wstring& user_input = StringToStdWString(input->value());
  if (webcore_type == WebCore::eventNames().DOMFocusOutEvent) {
    input_info.listener->OnBlur(input, user_input);
  } else if (webcore_type == WebCore::eventNames().inputEvent) {
    // Perform inline autocomplete if it is safe to do so.
    if (ShouldInlineAutocomplete(input,
                                 input_info.previous_text, user_input)) {
      input_info.listener->OnInlineAutocompleteNeeded(input, user_input);
    }
    // Update the info.
    input_info.previous_text = user_input;
    elements_info_[input] = input_info;
  } else {
    NOTREACHED() << "unexpected EventName for autocomplete listener";
  }
}

void AutocompleteBodyListener::AddInputListener(
    WebCore::HTMLInputElement* element,
    AutocompleteInputListener* listener) {
  DCHECK(elements_info_.find(element) == elements_info_.end());
  InputElementInfo elem_info;
  elem_info.listener = listener;
  elements_info_[element] = elem_info;
}

bool AutocompleteBodyListener::IsCaretAtEndOfText(
    WebCore::HTMLInputElement* element,
    size_t input_length,
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
  if (((element->selectionEnd() != element->selectionStart() + 1) ||
       (element->selectionEnd() != static_cast<int>(input_length))) &&
      ((element->selectionEnd() != element->selectionStart()) ||
       (element->selectionEnd() != static_cast<int>(previous_length)))) {
    return false;
  }
  return true;
}
}  // webkit_glue
