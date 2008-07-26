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

// A simple view that indicates to the user that a time-consuming operation
// is being performed, using a throbber and some explanatory text.

#ifndef CHROME_BROWSER_VIEWS_DELAY_VIEW_H__
#define CHROME_BROWSER_VIEWS_DELAY_VIEW_H__

#include "chrome/browser/controller.h"
#include "base/basictypes.h"
#include "chrome/views/label.h"
#include "chrome/views/native_button.h"
#include "chrome/views/throbber.h"

class DelayView : public ChromeViews::View,
                  public ChromeViews::NativeButton::Listener {
 public:
  // |text| explains the delay
  // |controller| receives notifications when the "cancel" button is pressed
  // |show_cancel| determines whether the cancel button is shown
  DelayView(const std::wstring& text,
            CommandController* controller,
            bool show_cancel);
  virtual ~DelayView();

  enum ViewID {
    ID_CANCEL = 10000,
  };

  // Overridden from ChromeViews::View
  virtual void Layout();

  // Implemented from ChromeViews::NativeButton::Listener
  virtual void ButtonPressed(ChromeViews::NativeButton *sender);

 private:
  CommandController* controller_;

  ChromeViews::Label* label_;
  ChromeViews::NativeButton* cancel_button_;
  ChromeViews::Throbber* throbber_;

  DISALLOW_EVIL_CONSTRUCTORS(DelayView);
};

#endif  // CHROME_BROWSER_VIEWS_DELAY_VIEW_H__
