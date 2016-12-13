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

#include "build/build_config.h"

class AutocompleteEditView;
class AutocompletePopupModel;
namespace gfx {
class Font;
class Rect;
}
#if defined(OS_WIN)
class AutocompleteEditViewWin;
class AutocompleteEditModel;
class Profile;
#endif

// An object in the browser UI can implement this interface to provide display
// bounds for the autocomplete popup view.
class AutocompletePopupPositioner {
 public:
  // Returns the bounds at which the popup should be shown, in screen
  // coordinates. The height is ignored, since the popup is sized to its
  // contents automatically.
  virtual gfx::Rect GetPopupBounds() const = 0;
};

class AutocompletePopupView {
 public:
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

  // Paint any pending updates.
  virtual void PaintUpdatesNow() = 0;

  // Returns the popup's model.
  virtual AutocompletePopupModel* GetModel() = 0;

#if defined(OS_WIN)
  // Create a popup view implementation. It may make sense for this to become
  // platform independent eventually.
  static AutocompletePopupView* CreatePopupView(
      const gfx::Font& font,
      AutocompleteEditViewWin* edit_view,
      AutocompleteEditModel* edit_model,
      Profile* profile,
      AutocompletePopupPositioner* popup_positioner);
#endif
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_VIEW_H_
