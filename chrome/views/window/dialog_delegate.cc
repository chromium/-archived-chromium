// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/window/dialog_delegate.h"

#include "base/logging.h"
#include "chrome/views/controls/button/native_button.h"
#include "chrome/views/window/window.h"

namespace views {

// Overridden from WindowDelegate:

int DialogDelegate::GetDefaultDialogButton() const {
  if (GetDialogButtons() & DIALOGBUTTON_OK)
    return DIALOGBUTTON_OK;
  if (GetDialogButtons() & DIALOGBUTTON_CANCEL)
    return DIALOGBUTTON_CANCEL;
  return DIALOGBUTTON_NONE;
}

View* DialogDelegate::GetInitiallyFocusedView() {
  // Focus the default button if any.
  DialogClientView* dcv = GetDialogClientView();
  int default_button = GetDefaultDialogButton();
  if (default_button == DIALOGBUTTON_NONE)
    return NULL;

  if ((default_button & GetDialogButtons()) == 0) {
    // The default button is a button we don't have.
    NOTREACHED();
    return NULL;
  }

  if (default_button & DIALOGBUTTON_OK)
    return dcv->ok_button();
  if (default_button & DIALOGBUTTON_CANCEL)
    return dcv->cancel_button();
  return NULL;
}

ClientView* DialogDelegate::CreateClientView(Window* window) {
  return new DialogClientView(window, GetContentsView());
}

DialogClientView* DialogDelegate::GetDialogClientView() const {
  ClientView* client_view = window()->GetClientView();
  DialogClientView* dialog_client_view = client_view->AsDialogClientView();
  DCHECK(dialog_client_view);
  return dialog_client_view;
}

}  // namespace views
