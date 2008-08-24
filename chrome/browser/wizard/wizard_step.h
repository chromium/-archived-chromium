// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

