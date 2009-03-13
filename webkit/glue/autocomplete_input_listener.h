// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines some infrastructure to handle inline autocomplete of DOM
// input elements.

#ifndef WEBKIT_GLUE_AUTOCOMPLETE_INPUT_LISTENER_H__
#define WEBKIT_GLUE_AUTOCOMPLETE_INPUT_LISTENER_H__

#include "config.h"
#include <map>
#include <string>

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "HTMLBodyElement.h"
#include "EventListener.h"
#include "HTMLInputElement.h"
MSVC_POP_WARNING();

#include "base/basictypes.h"

namespace WebCore {
class AtomicString;
class Event;
}

namespace webkit_glue {

// A proxy interface to a WebCore::HTMLInputElement for inline autocomplete.
// This class is NOT used directly by the AutocompleteInputListener but
// is included here since it is likely most listener implementations will
// want to interface with an HTMLInputElement (see PasswordACListener).
// The delegate does not own the WebCore element; it only interfaces it.
class HTMLInputDelegate {
 public:
  explicit HTMLInputDelegate(WebCore::HTMLInputElement* element);
  virtual ~HTMLInputDelegate();

  virtual void SetValue(const std::wstring& value);
  virtual void SetSelectionRange(size_t start, size_t end);
  virtual void OnFinishedAutocompleting();

 private:
  // The underlying DOM element we're wrapping. We reference the
  // underlying HTMLInputElement for its lifetime to ensure it does not get
  // freed by WebCore while in use by the delegate instance.
  WebCore::HTMLInputElement* element_;

  DISALLOW_COPY_AND_ASSIGN(HTMLInputDelegate);
};


class AutocompleteInputListener {
 public:
  virtual ~AutocompleteInputListener() { }

  // OnBlur: DOMFocusOutEvent occured, means one of two things.
  // 1. The user removed focus from the text field
  //    either by tabbing out or clicking;
  // 2. The page is being destroyed (e.g user closed the tab)
  virtual void OnBlur(WebCore::HTMLInputElement* input_element,
                      const std::wstring& user_input) = 0;

  // This method is called when there was a user-initiated text delta in
  // the edit field that now needs some inline autocompletion.
  // ShouldInlineAutocomplete gives the precondition for invoking this method.
  virtual void OnInlineAutocompleteNeeded(
      WebCore::HTMLInputElement* input_element,
      const std::wstring& user_input) = 0;
};

// The AutocompleteBodyListener class is a listener on the body element of a
// page that is responsible for reporting blur (for tab/click-out) and input
// events for form elements (registered by calling AddInputListener()).
// This allows to have one global listener for a page, as opposed to registering
// a listener for each form element, which impacts performances.
// Reasons for attaching to DOM directly rather than using EditorClient API:
// 1. Since we don't need to stop listening until the DOM node is unloaded,
//    it makes sense to use an object owned by the DOM node itself. Attaching
//    as a listener gives you this for free (nodes cleanup their listeners
//    upon destruction).
// 2. It allows fine-grained control when the popup/down is implemented
//    in handling key events / selecting elements.
class AutocompleteBodyListener : public WebCore::EventListener {
 public:
  // Constructs a listener for the specified frame.  It listens for blur and
  // input events for elements that are registered through the AddListener
  // method.
  // The listener is ref counted (because it inherits from
  // WebCore::EventListener).
  explicit AutocompleteBodyListener(WebCore::Frame* frame);
  virtual ~AutocompleteBodyListener();

  // Used by unit-tests.
  AutocompleteBodyListener() { }

  // Adds a listener for the specified |element|.
  // This call takes ownership of the |listener|.  Note that it is still OK to
  // add the same listener for different elements.
  void AddInputListener(WebCore::HTMLInputElement* element,
                        AutocompleteInputListener* listener);

  // EventListener implementation. Code that is common to inline autocomplete,
  // such as deciding whether or not it is safe to perform it, is refactored
  // into this method and the appropriate delegate method is invoked.
  virtual void handleEvent(WebCore::Event* event, bool is_window_event);

 protected:
  // Protected for mocking purposes.
  virtual bool IsCaretAtEndOfText(WebCore::HTMLInputElement* element,
                                  size_t input_length,
                                  size_t previous_length) const;

 private:
  // Determines, based on current state (previous_text_) and user input,
  // whether or not it is a good idea to attempt inline autocomplete.
  //
  // This method is based on firefox2 code in
  //   toolkit/components/autocomplete/src/nsAutoCompleteController.cpp
  // Its license block is:
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
  // The semantics of deciding whether or not the field is in a inline-
  // autocomplete-healthy state are summarized:
  // 1. The text is not identical to the text on the previous input event.
  // 2. This is not the result of a backspace.
  // 3. The text is not empty.
  // 4. The caret is at the end of the textbox.
  // TODO(timsteele): Examine autocomplete_edit.cc in the browser/ code and
  // make sure to capture all common exclusion cases here.
  bool ShouldInlineAutocomplete(WebCore::HTMLInputElement* input,
                                const std::wstring& old_text,
                                const std::wstring& new_text);

  // The data we store for each registered listener.
  struct InputElementInfo {
    InputElementInfo() : listener(NULL) { }
    AutocompleteInputListener* listener;
    std::wstring previous_text;
  };

  struct CmpRefPtrs {
    bool operator()(const RefPtr<WebCore::HTMLInputElement>& a,
                    const RefPtr<WebCore::HTMLInputElement>& b) const {
      return a.get() < b.get();
   }
  };

  typedef std::map<RefPtr<WebCore::HTMLInputElement>, InputElementInfo,
      CmpRefPtrs> InputElementInfoMap;
  InputElementInfoMap elements_info_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteBodyListener);
};

}  // webkit_glue

#endif  // WEBKIT_GLUE_AUTOCOMPLETE_INPUT_LISTENER_H__
