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
#pragma warning(push, 0)
#include "HTMLInputElement.h"
#include "HTMLFormElement.h"
#include "Document.h"
#include "Frame.h"
#include "Editor.h"
#include "EventNames.h"
#include "Event.h"
#include "EventListener.h"
#pragma warning(pop)

#undef LOG

#include "webkit/glue/autocomplete_input_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

using webkit_glue::AutocompleteInputListener;
using webkit_glue::AutocompleteEditDelegate;

class TestAutocompleteEditDelegate : public AutocompleteEditDelegate {
 public:
  TestAutocompleteEditDelegate() : caret_at_end_(false) {
  }

  virtual bool IsCaretAtEndOfText(size_t input_length,
                                  size_t prev_length) const {
    return caret_at_end_;
  }

  void SetCaretAtEnd(bool caret_at_end) {
    caret_at_end_ = caret_at_end;
  }

  virtual void SetSelectionRange(size_t start, size_t end) {
  }

  virtual void SetValue(const std::wstring& value) {
    value_ = value;
  }

  virtual std::wstring GetValue() const {
    return value_;
  }

  virtual void OnFinishedAutocompleting() {
  }

  void ResetTestState() {
    caret_at_end_ = false;
    value_.clear();
  }

 private:
  bool caret_at_end_;
  std::wstring value_;
};

class TestAutocompleteInputListener : public AutocompleteInputListener {
 public:
  TestAutocompleteInputListener(AutocompleteEditDelegate* d)
      : blurred_(false),
        did_request_inline_autocomplete_(false),
        AutocompleteInputListener(d) {
  }

  void ResetTestState() {
    blurred_ = false;
    did_request_inline_autocomplete_ = false;
  }

  bool blurred() const { return blurred_; }
  bool did_request_inline_autocomplete() const {
    return did_request_inline_autocomplete_;
  }

  virtual void OnBlur(const std::wstring& user_input) {
    blurred_ = true;
  }
  virtual void OnInlineAutocompleteNeeded(const std::wstring& user_input) {
    did_request_inline_autocomplete_ = true;
  }

 private:
  bool blurred_;
  bool did_request_inline_autocomplete_;
};

namespace {
class DomAutocompleteTests : public testing::Test {
 public:
  void SetUp() {
    WebCore::EventNames::init();
  }

  void FireAndHandleInputEvent(AutocompleteInputListener* listener) {
    WebCore::Event event(WebCore::EventNames::inputEvent, false, false);
    listener->handleEvent(&event, false);
  }

  void SimulateTypedInput(TestAutocompleteEditDelegate* delegate,
                          AutocompleteInputListener* listener,
                          const std::wstring& new_input) {
      delegate->SetValue(new_input);
      delegate->SetCaretAtEnd(true);
      FireAndHandleInputEvent(listener);
  }
};
}  // namespace

TEST_F(DomAutocompleteTests, OnBlur) {
  // Simulate a blur event and ensure it is properly dispatched.
  // Listener takes ownership of its delegate.
  TestAutocompleteInputListener listener(new TestAutocompleteEditDelegate());
  WebCore::Event event(WebCore::EventNames::DOMFocusOutEvent, false, false);
  listener.handleEvent(&event, false);
  EXPECT_TRUE(listener.blurred());
}

TEST_F(DomAutocompleteTests, InlineAutocompleteTriggeredByInputEvent) {
  // Set up the edit delegate, assuming the field was initially empty.
  TestAutocompleteEditDelegate* delegate = new TestAutocompleteEditDelegate();
  TestAutocompleteInputListener listener(delegate);

  // Simulate an inputEvent by setting the value and artificially firing evt.
  // The user typed 'g'.
  SimulateTypedInput(delegate, &listener, L"g");
  EXPECT_TRUE(listener.did_request_inline_autocomplete());
}

TEST_F(DomAutocompleteTests, InlineAutocompleteHeuristics) {
  TestAutocompleteEditDelegate* delegate = new TestAutocompleteEditDelegate();
  TestAutocompleteInputListener listener(delegate);

  // Simulate a user entering some text, and then backspacing to remove
  // a character.
  SimulateTypedInput(delegate, &listener, L"g");
  EXPECT_TRUE(listener.did_request_inline_autocomplete());
  listener.ResetTestState();

  SimulateTypedInput(delegate, &listener, L"go");
  EXPECT_TRUE(listener.did_request_inline_autocomplete());
  listener.ResetTestState();

  SimulateTypedInput(delegate, &listener, L"g");
  EXPECT_FALSE(listener.did_request_inline_autocomplete());
  listener.ResetTestState();

  // Now simulate the user moving the cursor to a position other than the end,
  // and adding text.
  delegate->SetCaretAtEnd(false);
  delegate->SetValue(L"og");
  FireAndHandleInputEvent(&listener);
  EXPECT_FALSE(listener.did_request_inline_autocomplete());
  listener.ResetTestState();

  // Test that same input doesn't trigger autocomplete.
  delegate->SetCaretAtEnd(true);
  delegate->SetValue(L"og");
  FireAndHandleInputEvent(&listener);
  EXPECT_FALSE(listener.did_request_inline_autocomplete());
  listener.ResetTestState();
}

