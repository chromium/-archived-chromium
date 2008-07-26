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

#ifndef CHROME_BROWSER_WIZARD_WIZARD_H__
#define CHROME_BROWSER_WIZARD_WIZARD_H__

#include <vector>

#include "base/values.h"
#include "chrome/views/view.h"

class WizardView;
class WizardStep;
class SkBitmap;

////////////////////////////////////////////////////////////////////////////////
//
// A WizardDelegate can receive a notification when the wizard session is done.
//
////////////////////////////////////////////////////////////////////////////////
class WizardDelegate {
 public:
  // Inform the delegate that the user closed the wizard. if commit is true,
  // the current wizard state contains the changes.
  virtual void WizardClosed(bool commit) = 0;

  // Inform the delegate that the containing window should be resized such as
  // the top level wizard view as returned by GetTopLevelView() has the provided
  // width and height.
  virtual void ResizeTopLevelView(int width, int height) = 0;
};

////////////////////////////////////////////////////////////////////////////////
//
// Wizard is the main entry point for Chrome's wizard framework. The wizard can
// be configured with several WizardSteps.
//
// The Wizard returns a top level view that is typically integrated inside some
// constrained dialog.
//
////////////////////////////////////////////////////////////////////////////////
class Wizard {
 public:

  Wizard(WizardDelegate* delegate);
  ~Wizard();

  // Set the wizard state. The state is owned by the receiving wizard
  // instance.
  void SetState(DictionaryValue* state);

  // Return the wizard current wizard state.
  DictionaryValue* GetState();

  // Add a wizard step. The step is owned by the wizard.
  void AddStep(WizardStep* s);

  // Return the number of step.
  int GetStepCount() const;

  // Return the step at the provided index.
  WizardStep* GetStepAt(int index);

  // Remove all the steps.
  void RemoveAllSteps();

  // Return the wizard top level view.
  ChromeViews::View* GetTopLevelView();

  // Start a wizard session. At this point, the top level view is expected to
  // be inserted into a visible view hierarchy.
  void Start();

  // Aborts the current wizard session if it is running. The delegate is
  // notified. This method does nothing if the wizard is not running.
  void Abort();

  // Specify an SkBitmap to be associated with the provided key. This is useful
  // to help wizard step implementors only fetch or load an image once when it
  // is on several steps. The image is owned by the wizard.
  void SetImage(const std::wstring& image_name, SkBitmap* image);

  // Returns the image for the provided key or NULL if it doesn't exist. If the
  // image exists and |remove_image| is true, the image will also be removed
  // from the list of images maintained by the wizard and the caller will be
  // given ownership of the bitmap.
  SkBitmap* GetImage(const std::wstring& image_name, bool remove_image);

  // Step navigation
  void SelectNextStep();
  void SelectPreviousStep();

  // Invoked when the current step WizardNavigationDescriptor returned from
  // GetNavigationDescriptor() changes. Call this method to refresh the wizard
  // buttons. This method does nothing if the current WizardNavigationDescriptor
  // is NULL.
  void NavigationDescriptorChanged();

  // Change whether the next step can be selected by the user.
  void EnableNextStep(bool flag);

  // Checks whether the next step can be selected by the user.
  bool IsNextStepEnabled() const;

  // Change whether the previous step can be selected by the user.
  void EnablePreviousStep(bool flag);

  // Checks whether the previous step can be selected by the user.
  bool IsPreviousStepEnabled() const;

 private:
  friend class WizardView;

  // Select the step at the provided index. Note the caller is responsible for
  // calling WillBecomeInvisible. We do this because we need the current step
  // to finish editing before knowing which step to select next.
  void SelectStepAt(int index);

  // Inform the wizard that the current session is over. This will reset the
  // selected step and detach the main view from its container.
  void Reset();

  // Invoked by the WizardView in response to button pressed.
  void Cancel();

  // Internal step navigation.
  void SelectStep(bool is_forward);

  // Abort the wizard if it is running. Notify the delegate
  void WizardDone(bool commit);

  WizardView* view_;

  typedef std::vector<WizardStep*> WizardStepVector;
  WizardStepVector steps_;

  WizardDelegate* delegate_;

  DictionaryValue* state_;

  int selected_step_;

  bool is_running_;

  typedef std::map<std::wstring, SkBitmap*> ImageMap;
  ImageMap images_;

  DISALLOW_EVIL_CONSTRUCTORS(Wizard);
};
#endif  // CHROME_BROWSER_WIZARD_WIZARD_H__
