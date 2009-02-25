// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_MODAL_DIALOG_QUEUE_H__
#define CHROME_BROWSER_APP_MODAL_DIALOG_QUEUE_H__

#include <queue>

#include "chrome/browser/app_modal_dialog_delegate.h"

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
  static void AddDialog(AppModalDialogDelegate* dialog);

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

  // Returns true if there is currently an active app modal dialog box.
  static bool HasActiveDialog() {
    return active_dialog_ != NULL;
  }

  // Accessor for |active_dialog_|.
  static AppModalDialogDelegate* active_dialog() { return active_dialog_; }

 private:
  // Shows |dialog| and notifies the BrowserList that a modal dialog is showing.
  static void ShowModalDialog(AppModalDialogDelegate* dialog);

  // Contains all app modal dialogs which are waiting to be shown, with the
  // currently modal dialog at the front of the queue.
  static std::queue<AppModalDialogDelegate*>*
      app_modal_dialog_queue_;

  // The currently active app-modal dialog box's delegate. NULL if there is no
  // active app-modal dialog box.
  static AppModalDialogDelegate* active_dialog_;
};

#endif // CHROME_BROWSER_APP_MODAL_DIALOG_QUEUE_H__

