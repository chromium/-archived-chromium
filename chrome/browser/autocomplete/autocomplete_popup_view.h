// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the interface class AutocompletePopupView.  Each toolkit
// will implement the popup view differently, so that code is inheriently
// platform specific.  However, the AutocompletePopupModel needs to do some
// communication with the view.  Since the model is shared between platforms,
// we need to define an interface that all view implementations will share.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_VIEW_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_VIEW_H_

#include "base/basictypes.h"

class AutocompleteEditView;
class AutocompletePopupModel;
class ChromeFont;

class AutocompletePopupView {
 public:
  static AutocompletePopupView* CreatePopupView(
      AutocompletePopupModel* model,
      const ChromeFont& font,
      AutocompleteEditView* edit_view);

  virtual ~AutocompletePopupView() { }

  // Returns true if the popup is currently open.
  virtual bool IsOpen() const = 0;

  // Invalidates one line of the autocomplete popup.
  virtual void InvalidateLine(size_t line) = 0;

  // Redraws the popup window to match any changes in the result set; this may
  // mean opening or closing the window.
  virtual void UpdatePopupAppearance() = 0;

  // Called by the model when hover is enabled or disabled.
  virtual void OnHoverEnabledOrDisabled(bool disabled) = 0;

#if defined(OS_WIN)
  // Cause a WM_PAINT immediately (see msdn on UpdateWindow()).
  virtual void PaintUpdatesNow() = 0;
#endif
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_VIEW_H_
