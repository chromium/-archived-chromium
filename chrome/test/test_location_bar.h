// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TEST_LOCATION_BAR_H_
#define CHROME_TEST_TEST_LOCATION_BAR_H_

#include "chrome/browser/location_bar.h"
#include "chrome/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class TestLocationBar : public LocationBar {
 public:
  TestLocationBar()
      : disposition_(CURRENT_TAB),
        transition_(PageTransition::LINK) {
  }

  void set_input_string(const std::wstring& input_string) {
    input_string_ = input_string;
  }
  void set_disposition(WindowOpenDisposition disposition) {
    disposition_ = disposition;
  }
  void set_transition(PageTransition::Type transition) {
    transition_ = transition;
  }

  // Overridden from LocationBar:
  virtual void ShowFirstRunBubble(bool use_OEM_bubble) {}
  virtual std::wstring GetInputString() const { return input_string_; }
  virtual WindowOpenDisposition GetWindowOpenDisposition() const {
    return disposition_;
  }
  virtual PageTransition::Type GetPageTransition() const { return transition_; }
  virtual void AcceptInput() {}
  virtual void AcceptInputWithDisposition(WindowOpenDisposition) {}
  virtual void FocusLocation() {}
  virtual void FocusSearch() {}
  virtual void UpdatePageActions() {}
  virtual void SaveStateToContents(TabContents* contents) {}
  virtual void Revert() {}
  virtual AutocompleteEditView* location_entry() {
    return NULL;
  }
  virtual LocationBarTesting* GetLocationBarForTesting() {
    return NULL;
  }

 private:

  // Test-supplied values that will be returned through the LocationBar
  // interface.
  std::wstring input_string_;
  WindowOpenDisposition disposition_;
  PageTransition::Type transition_;

  DISALLOW_COPY_AND_ASSIGN(TestLocationBar);
};


#endif  // CHROME_TEST_TEST_LOCATION_BAR_H_
