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

#ifndef CHROME_BROWSER_VIEWS_FIRST_RUN_VIEW_H__
#define CHROME_BROWSER_VIEWS_FIRST_RUN_VIEW_H__

#include "chrome/browser/views/first_run_view_base.h"
#include "chrome/browser/views/first_run_customize_view.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/link.h"
#include "chrome/views/view.h"

namespace ChromeViews {

class Label;
class Window;

}

class Profile;
class ImporterHost;

// FirstRunView implements the dialog that welcomes to user to Chrome after
// a fresh install.
class FirstRunView : public FirstRunViewBase,
                     public ChromeViews::LinkController,
                     public FirstRunCustomizeView::CustomizeViewObserver {
 public:
  explicit FirstRunView(Profile* profile);
  virtual ~FirstRunView();

  // Overridden from ChromeViews::View:
  virtual void GetPreferredSize(CSize *out);
  virtual void Layout();

  // Overridden from ChromeViews::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual bool Accept();
  virtual bool Cancel();

  // Overridden from ChromeViews::WindowDelegate:
  virtual std::wstring GetWindowTitle() const;

  // Overridden from ChromeViews::LinkActivated:
  virtual void LinkActivated(ChromeViews::Link* source, int event_flags);

  // Overridden from FirstRunCustomizeView:
  virtual void CustomizeAccepted();
  virtual void CustomizeCanceled();

 private:
  // Initializes the controls on the dialog.
  void SetupControls();

  // Creates the dialog that allows the user to customize work items.
  void OpenCustomizeDialog();

  ChromeViews::Label* welcome_label_;
  ChromeViews::Label* actions_label_;
  ChromeViews::Label* actions_import_;
  ChromeViews::Label* actions_shorcuts_;
  ChromeViews::Link* customize_link_;
  bool customize_selected_;

  DISALLOW_EVIL_CONSTRUCTORS(FirstRunView);
};

#endif  // CHROME_BROWSER_VIEWS_FIRST_RUN_VIEW_H__
