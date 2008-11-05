// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines some infrastructure to handle inline autocomplete of DOM
// input elements.

#ifndef WEBKIT_GLUE_AUTOCOMPLETE_INPUT_LISTENER_H__
#define WEBKIT_GLUE_AUTOCOMPLETE_INPUT_LISTENER_H__

#include <string>
#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "EventListener.h"
MSVC_POP_WARNING();

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

namespace WebCore {
class AtomicString;
class Event;
class HTMLInputElement;
}

namespace webkit_glue {

// This interface exposes all required functionality to perform inline
// autocomplete on an edit field.
class AutocompleteEditDelegate {
 public:
  // Virtual destructor so it is safe to delete through AutocompleteEditDelegate
  // pointers.	 
  virtual ~AutocompleteEditDelegate() {
  }

  // Whether or not the caret/selection is at the end of input.
  // input_length gives the length of the user-typed input.
  // previous_length is the length of the previously typed input.
  virtual bool IsCaretAtEndOfText(size_t input_length,
                                  size_t previous_length) const = 0;

  // Set the selected range of text that should be displayed to the user
  // to [start,end] (inclusive).
  virtual void SetSelectionRange(size_t start, size_t end) = 0;

  // Accessor/Mutator for the text value.
  virtual void SetValue(const std::wstring& value) = 0;
  virtual std::wstring GetValue() const = 0;

  // Called when processing is finished.
  virtual void OnFinishedAutocompleting() = 0;
};

// A proxy interface to a WebCore::HTMLInputElement for inline autocomplete.
// This class is NOT used directly by the AutocompleteInputListener but
// is included here since it is likely most listener implementations will
// want to interface with an HTMLInputElement (see PasswordACListener).
// The delegate does not own the WebCore element; it only interfaces it.
class HTMLInputDelegate : public AutocompleteEditDelegate {
 public:
  explicit HTMLInputDelegate(WebCore::HTMLInputElement* element);
  virtual ~HTMLInputDelegate();

  // AutocompleteEditDelegate implementation.
  virtual bool IsCaretAtEndOfText(size_t input_length,
                                  size_t previous_length) const;
  virtual void SetValue(const std::wstring& value);
  virtual std::wstring GetValue() const;
  virtual void SetSelectionRange(size_t start, size_t end);
  virtual void OnFinishedAutocompleting();

 private:
  // The underlying DOM element we're wrapping. We reference the 
  // underlying HTMLInputElement for its lifetime to ensure it does not get
  // freed by WebCore while in use by the delegate instance.
  WebCore::HTMLInputElement* element_;

  DISALLOW_EVIL_CONSTRUCTORS(HTMLInputDelegate);
};

// Reasons for attaching to DOM directly rather than using EditorClient API:
// 1. Since we don't need to stop listening until the DOM node is unloaded,
//    it makes sense to use an object owned by the DOM node itself. Attaching
//    as a listener gives you this for free (nodes cleanup their listeners
//    upon destruction).
// 2. It allows fine-grained control when the popup/down is implemented
//    in handling key events / selecting elements.
//
// Note: The element an AutocompleteInputListener is attached to is kept
//       separate from the element it explicitly interacts with (via the
//       AutocompleteEditDelegate API) so that the listeners are effectively
//       decoupled from HTMLInputElement; which is great when it comes time
//       for testing. The listeners can be constructed using only the delegates,
//       and events can be manually fired to test specific behaviour.
//
class AutocompleteInputListener : public WebCore::EventListener {
 public:
  // Construct a listener with access to an edit field (i.e an HTMLInputElement)
  // through a delegate, so that it can obtain and set values needed for
  // autocomplete. See the above Note which explains why the edit_delegate it
  // is handed here is not necessarily the node it is attached to as an
  // EventListener. This object takes ownership of its edit_delegate.
  explicit AutocompleteInputListener(AutocompleteEditDelegate* edit_delegate);

  virtual ~AutocompleteInputListener() {
  }

  // EventListener implementation. Code that is common to inline autocomplete,
  // such as deciding whether or not it is safe to perform it, is refactored
  // into this method and the appropriate delegate method is invoked.
  virtual void handleEvent(WebCore::Event* event, bool is_window_event);

  // Subclasses need only implement the following two methods. They could
  // be declared protected but are left public to be invoked by testing.

  // OnBlur: DOMFocusOutEvent occured, means one of two things.
  // 1. The user removed focus from the text field
  //    either by tabbing out or clicking;
  // 2. The page is being destroyed (e.g user closed the tab)
  virtual void OnBlur(const std::wstring& user_input) = 0;

  // This method is called when there was a user-initiated text delta in
  // the edit field that now needs some inline autocompletion.
  // ShouldInlineAutocomplete gives the precondition for invoking this method.
  virtual void OnInlineAutocompleteNeeded(const std::wstring& user_input) = 0;

 protected:
  // Access and modify the edit field only via the AutocompleteEditDelegate API.
  AutocompleteEditDelegate* edit_delegate() { return edit_delegate_.get(); }

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
  bool ShouldInlineAutocomplete(const std::wstring& user_input);

  // For testability, the AutocompleteEditDelegate API is used to decouple
  // AutocompleteInputListeners from underlying HTMLInputElements. This
  // allows testcases to mock delegates and test the autocomplete code without
  // a real underlying DOM.
  scoped_ptr<AutocompleteEditDelegate> edit_delegate_;

  // Stores the text across input events during inline autocomplete.
  std::wstring previous_text_;

  DISALLOW_EVIL_CONSTRUCTORS(AutocompleteInputListener);
};

// Attach the listener as an EventListener to basic events required
// to handle inline autocomplete (and blur events for tab/click-out).
// Attaching to the WebCore element effectively transfers ownership of
// the listener objects. When WebCore is tearing down the document,
// any attached listeners are destroyed.
// See Document::removeAllEventListenersFromAllNodes which is called by
// FrameLoader::stopLoading. Also, there is no need for  matching calls to
// removeEventListener because the simplest and most convienient thing to do
// for autocompletion is to stop listening once the element is destroyed.
void AttachForInlineAutocomplete(WebCore::HTMLInputElement* target,
                                 AutocompleteInputListener* listener);

}  // webkit_glue

#endif  // WEBKIT_GLUE_AUTOCOMPLETE_INPUT_LISTENER_H__

