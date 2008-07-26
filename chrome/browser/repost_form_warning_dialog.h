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

#ifndef CHROME_BROWSER_REPOST_FORM_WARNING_H__
#define CHROME_BROWSER_REPOST_FORM_WARNING_H__

#include "chrome/common/notification_service.h"
#include "chrome/views/dialog_delegate.h"

class MessageBoxView;
class NavigationController;
namespace ChromeViews {
class Window;
}

class RepostFormWarningDialog : public ChromeViews::DialogDelegate,
                                public NotificationObserver {
 public:
  // Creates and runs a message box which asks the user if they want to resend
  // an HTTP POST.
  static void RunRepostFormWarningDialog(
      NavigationController* navigation_controller);
  virtual ~RepostFormWarningDialog();

  // ChromeViews::DialogDelegate Methods:
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual void WindowClosing();
  virtual bool Cancel();
  virtual bool Accept();

  // ChromeViews::WindowDelegate Methods:
  virtual bool IsModal() const { return true; }

 private:
  // Use RunRepostFormWarningDialog to use.
  RepostFormWarningDialog(NavigationController* navigation_controller);

  // NotificationObserver implementation.
  // Watch for a new load or a closed tab and dismiss the dialog if they occur.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // The message box view whose commands we handle.
  MessageBoxView* message_box_view_;

  // Navigation controller, used to continue the reload.
  NavigationController* navigation_controller_;

  // Our window. Used to close the dialog if we navigate or lose our navigation
  // controller before the user makes a decision.
  ChromeViews::Window* dialog_;

  DISALLOW_EVIL_CONSTRUCTORS(RepostFormWarningDialog);
};

#endif // CHROME_BROWSER_REPOST_FORM_WARNING_H__
