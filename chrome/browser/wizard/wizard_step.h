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

#ifndef CHROME_BROWSER_WIZARD_WIZARD_STEP_H
#define CHROME_BROWSER_WIZARD_WIZARD_STEP_H

#include <string>
#include <vector>

namespace ChromeViews {
class View;
}
class Wizard;

// A navigation descriptor allows wizard steps to describe custom navigation
// buttons and offsets (how many step forward or backward).
struct WizardNavigationDescriptor {
 public:
  WizardNavigationDescriptor() : custom_default_button_(false),
                                 default_offset_(0),
                                 custom_alternate_button_(false),
                                 alternate_offset_(0),
                                 can_cancel_(true) {
  }

  ~WizardNavigationDescriptor() {}

  //
  // Default button definition.
  //

  // true if the steps wants a custom default button.
  bool custom_default_button_;

  // Default button label or empty string to remove the button completely.
  std::wstring default_label_;

  // Positive or negative offset to the step that should become selected when
  // the default button is pressed.
  int default_offset_;

  //
  // Alternate button definition.
  //

  // true if the steps wants a custom alternate button.
  bool custom_alternate_button_;

  // Alternate button label or empty string to remove the button completelty.
  // This button is typically used for "previous"
  std::wstring alternate_label_;

  // Positive or negative offset to the step that should become selected
  // when the alternate button is pressed.
  int alternate_offset_;

  // Whether the step features a cancel button.
  bool can_cancel_;

 private:

  DISALLOW_EVIL_CONSTRUCTORS(WizardNavigationDescriptor);
};

////////////////////////////////////////////////////////////////////////////////
//
// A WizardStep instance represents a single wizard step.
//
////////////////////////////////////////////////////////////////////////////////
class WizardStep {
 public:
  enum StepAction {
    DEFAULT_ACTION = 0, // Usually next
    ALTERNATE_ACTION = 1, // Usually previous
    CANCEL_ACTION = 2  // The cancel button has been pressed.
  };

  // Return the title for this state. If an empty string is returned, no title
  // will be visible.
  virtual const std::wstring GetTitle(Wizard* wizard) = 0;

  // Return the view for this state. The view is owned by the step
  virtual ChromeViews::View* GetView(Wizard* wizard) = 0;

  // Return whether this step is enabled given the provided wizard state.
  // If the step returns false it won't be shown in the flow.
  virtual bool IsEnabledFor(Wizard* wizard) = 0;

  // Inform the step that it is now visible. The step view has been added to a
  // view hierarchy.
  virtual void DidBecomeVisible(Wizard* wizard) = 0;

  // Inform the step that it is about to become invisible. Any
  // change pending in the UI should be flushed. |action| defines what button
  // the user clicked. (see above)
  virtual void WillBecomeInvisible(Wizard* wizard, StepAction action) = 0;

  // Fill lines with some human readable text describing what this step will do
  // The provided strings are owned by the caller.
  virtual void GetSummary(Wizard* wizard,
                          std::vector<std::wstring>* lines) = 0;

  // Dispose this step. This should delete the step.
  virtual void Dispose() = 0;

  // Return a custom wizard navigation descriptor. This method can return NULL
  // to simply use the default buttons. The returned descriptor is owned by
  // the receiver and is assumed to be valid as long as the receiver is
  // visible. Call Wizard::NavigationDescriptorChanged() if you need to change
  // the navigation buttons while the wizard step is visible.
  virtual WizardNavigationDescriptor* GetNavigationDescriptor() = 0;
};

#endif  // CHROME_BROWSER_WIZARD_WIZARD_STEP_H
