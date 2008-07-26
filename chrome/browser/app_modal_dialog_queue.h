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

#ifndef CHROME_BROWSER_APP_MODAL_DIALOG_QUEUE_H__
#define CHROME_BROWSER_APP_MODAL_DIALOG_QUEUE_H__

#include <queue>

#include "chrome/views/app_modal_dialog_delegate.h"

// Keeps a queue of AppModalDialogDelegates, making sure only one app modal
// dialog is shown at a time.
class AppModalDialogQueue {
 public:
  // Adds a modal dialog to the queue, if there are no other dialogs in the
  // queue, the dialog will be shown immediately. Once it is shown, the
  // most recently active browser window (or whichever is currently active)
  // will be app modal, meaning it will be activated if the user tries to
  // activate any other browser windows. So the dialog being shown should
  // assure it is the child of BrowserList::GetLastActive() so that it is
  // activated as well. See browser_list.h for more notes about our somewhat
  // sloppy app modality.
  // Note: The AppModalDialogDelegate |dialog| must be window modal before it
  // can be added as app modal.
  static void AddDialog(ChromeViews::AppModalDialogDelegate* dialog);

  // Removes the current dialog in the queue (the one that is being shown).
  // Shows the next dialog in the queue, if any is present. This does not
  // ensure that the currently showing dialog is closed, it just makes it no
  // longer app modal.
  static void ShowNextDialog();

  // Activates and shows the current dialog, if the user clicks on one of the
  // windows disabled by the presence of an app modal dialog. This forces
  // the window to be visible on the display even if desktop manager software
  // opened the dialog on another virtual desktop. Assumes there is currently a
  // dialog being shown. (Call BrowserList::IsShowingAppModalDialog to test
  // this condition).
  static void ActivateModalDialog();

 private:
  // Shows |dialog| and notifies the BrowserList that a modal dialog is showing.
  static void ShowModalDialog(ChromeViews::AppModalDialogDelegate* dialog);

  // Contains all app modal dialogs which are waiting to be shown, with the
  // currently modal dialog at the front of the queue.
  static std::queue<ChromeViews::AppModalDialogDelegate*>*
      app_modal_dialog_queue_;
};

#endif // CHROME_BROWSER_APP_MODAL_DIALOG_QUEUE_H__
