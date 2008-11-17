// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DomAutocompleteTests in this file are responsible for ensuring the
// abstract dom autocomplete framework is correctly responding to events and
// delegating to appropriate places. This means concrete implementations should
// focus only on testing the code actually written for that implementation and
// those tests should be completely decoupled from WebCore::Event.

#include <string>

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "HTMLInputElement.h"
#include "HTMLFormElement.h"
#include "Document.h"
#include "Frame.h"
#include "Editor.h"
#include "EventNames.h"
#include "Event.h"
#include "EventListener.h"
#include <wtf/Threading.h>
MSVC_POP_WARNING();

#undef LOG

#include "webkit/glue/autocomplete_input_listener.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using WebCore::Event;

namespace webkit_glue {

class TestAutocompleteBodyListener : public AutocompleteBodyListener {
 public:
  TestAutocompleteBodyListener() {
  }

  void SetCaretAtEnd(WebCore::HTMLInputElement* element, bool value) {
    std::vector<WebCore::HTMLInputElement*>::iterator iter =
        std::find(caret_at_end_elements_.begin(), caret_at_end_elements_.end(),
                  element);
    if (value) {
      if (iter == caret_at_end_elements_.end())
        caret_at_end_elements_.push_back(element);
    } else {
      if (iter != caret_at_end_elements_.end())
        caret_at_end_elements_.erase(iter);
    }
  }

  void ResetTestState() {
    caret_at_end_elements_.clear();
  }

 protected:
  // AutocompleteBodyListener override.
  virtual bool IsCaretAtEndOfText(WebCore::HTMLInputElement* element,
                                  size_t input_length,
                                  size_t previous_length) const {
    return std::find(caret_at_end_elements_.begin(), 
                     caret_at_end_elements_.end(),
                     element) != caret_at_end_elements_.end();
  }

 private:
   // List of elements for which the caret is at the end of the text.
   std::vector<WebCore::HTMLInputElement*> caret_at_end_elements_;
};

class TestAutocompleteInputListener : public AutocompleteInputListener {
 public:
  TestAutocompleteInputListener()
      : blurred_(false),
        did_request_inline_autocomplete_(false) {
  }

  void ResetTestState() {
    blurred_ = false;
    did_request_inline_autocomplete_ = false;
  }

  bool blurred() const { return blurred_; }
  bool did_request_inline_autocomplete() const {
    return did_request_inline_autocomplete_;
  }

  virtual void OnBlur(WebCore::HTMLInputElement* element,
                      const std::wstring& user_input) {
    blurred_ = true;
  }
  virtual void OnInlineAutocompleteNeeded(WebCore::HTMLInputElement* element,
                                          const std::wstring& user_input) {
    did_request_inline_autocomplete_ = true;
  }

 private:
  bool blurred_;
  bool did_request_inline_autocomplete_;
};

namespace {

class DomAutocompleteTests : public TestShellTest {
 public:
  virtual void SetUp() {
    TestShellTest::SetUp();
    // We need a document in order to create HTMLInputElements.
    WebView* view = test_shell_->webView();
    WebFrameImpl* frame = static_cast<WebFrameImpl*>(view->GetMainFrame());
    document_ = frame->frame()->document();
  }

  void FireAndHandleInputEvent(AutocompleteBodyListener* listener,
                               WebCore::HTMLInputElement* element) {
    RefPtr<Event> event(Event::create(WebCore::eventNames().inputEvent,
                                      false, false));
    event->setTarget(element);
    listener->handleEvent(event.get(), false);
  }

  void SimulateTypedInput(TestAutocompleteBodyListener* listener,
                          WebCore::HTMLInputElement* element,
                          const std::wstring& new_input,
                          bool caret_at_end) {
      element->setValue(StdWStringToString(new_input));
      listener->SetCaretAtEnd(element, caret_at_end);
      FireAndHandleInputEvent(listener, element);
  }

  WebCore::Document* document_;
};
}  // namespace

TEST_F(DomAutocompleteTests, OnBlur) {
  RefPtr<WebCore::HTMLInputElement> ignored_element =
      new WebCore::HTMLInputElement(document_);
  RefPtr<WebCore::HTMLInputElement> listened_element =
      new WebCore::HTMLInputElement(document_);
  RefPtr<TestAutocompleteBodyListener> body_listener =
      new TestAutocompleteBodyListener;
  TestAutocompleteInputListener* listener = new TestAutocompleteInputListener();
  // body_listener takes ownership of the listener.
  body_listener->AddInputListener(listened_element.get(), listener);

  // Simulate a blur event to the element we are not listening to.
  // Our listener should not be notified.
  RefPtr<Event> event(Event::create(WebCore::eventNames().DOMFocusOutEvent,
                                    false, false));
  event->setTarget(ignored_element.get());
  body_listener->handleEvent(event.get(), false);
  EXPECT_FALSE(listener->blurred());
  
  // Now simulate the event on the input element we are listening to.
  event->setTarget(listened_element.get());
  body_listener->handleEvent(event.get(), false);
  EXPECT_TRUE(listener->blurred());
}

TEST_F(DomAutocompleteTests, InlineAutocompleteTriggeredByInputEvent) {
  RefPtr<WebCore::HTMLInputElement> ignored_element =
      new WebCore::HTMLInputElement(document_);
  RefPtr<WebCore::HTMLInputElement> listened_element =
      new WebCore::HTMLInputElement(document_);
  RefPtr<TestAutocompleteBodyListener> body_listener =
      new TestAutocompleteBodyListener;

  TestAutocompleteInputListener* listener = new TestAutocompleteInputListener();
  body_listener->AddInputListener(listened_element.get(), listener);

  // Simulate an inputEvent by setting the value and artificially firing evt.
  // The user typed 'g'.
  SimulateTypedInput(body_listener.get(), ignored_element.get(), L"g", true);
  EXPECT_FALSE(listener->did_request_inline_autocomplete());
  SimulateTypedInput(body_listener.get(), listened_element.get(), L"g", true);
  EXPECT_TRUE(listener->did_request_inline_autocomplete());
}

TEST_F(DomAutocompleteTests, InlineAutocompleteHeuristics) {
  RefPtr<WebCore::HTMLInputElement> input_element =
      new WebCore::HTMLInputElement(document_);
  RefPtr<TestAutocompleteBodyListener> body_listener =
      new TestAutocompleteBodyListener();

  TestAutocompleteInputListener* listener = new TestAutocompleteInputListener();
  body_listener->AddInputListener(input_element.get(), listener);

  // Simulate a user entering some text, and then backspacing to remove
  // a character.
  SimulateTypedInput(body_listener.get(), input_element.get(), L"g", true);
  EXPECT_TRUE(listener->did_request_inline_autocomplete());
  listener->ResetTestState();
  body_listener->ResetTestState();

  SimulateTypedInput(body_listener.get(), input_element.get(), L"go", true);
  EXPECT_TRUE(listener->did_request_inline_autocomplete());
  listener->ResetTestState();
  body_listener->ResetTestState();

  SimulateTypedInput(body_listener.get(), input_element.get(), L"g", true);
  EXPECT_FALSE(listener->did_request_inline_autocomplete());
  listener->ResetTestState();
  body_listener->ResetTestState();

  // Now simulate the user moving the cursor to a position other than the end,
  // and adding text.
  SimulateTypedInput(body_listener.get(), input_element.get(), L"og", false);
  EXPECT_FALSE(listener->did_request_inline_autocomplete());
  listener->ResetTestState();
  body_listener->ResetTestState();

  // Test that same input doesn't trigger autocomplete.
  SimulateTypedInput(body_listener.get(), input_element.get(), L"og", true);
  EXPECT_FALSE(listener->did_request_inline_autocomplete());
  listener->ResetTestState();
  body_listener->ResetTestState();
}

}  // webkit_glue
