// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/dialog_delegate.h"

#include "chrome/views/window.h"

namespace views {

// Overridden from WindowDelegate:
View* DialogDelegate::GetInitiallyFocusedView() const {
  // Try to focus the OK then the Cancel button if present.
  DialogClientView* dcv = GetDialogClientView();
  if (GetDialogButtons() & DIALOGBUTTON_OK)
    return dcv->ok_button();
  if (GetDialogButtons() & DIALOGBUTTON_CANCEL)
    return dcv->cancel_button();
  return NULL;
}

ClientView* DialogDelegate::CreateClientView(Window* window) {
  return new DialogClientView(window, GetContentsView());
}

DialogClientView* DialogDelegate::GetDialogClientView() const {
  ClientView* client_view = window()->client_view();
  DialogClientView* dialog_client_view = client_view->AsDialogClientView();
  DCHECK(dialog_client_view);
  return dialog_client_view;
}

}  // namespace views

