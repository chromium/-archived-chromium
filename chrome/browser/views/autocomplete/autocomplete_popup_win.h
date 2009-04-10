// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_
#define CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_

#include "chrome/browser/autocomplete/autocomplete_popup_view.h"
#include "chrome/views/widget/widget_win.h"

class AutocompleteEditModel;
class AutocompleteEditViewWin;
class Profile;

class AutocompletePopupWin : public views::WidgetWin,
                             public AutocompletePopupView {
 public:
  AutocompletePopupWin(const ChromeFont& font,
                       AutocompleteEditViewWin* edit_view,
                       AutocompleteEditModel* edit_model,
                       Profile* profile);
  virtual ~AutocompletePopupWin();

  // Overridden from AutocompletePopupView:
  virtual bool IsOpen() const;
  virtual void InvalidateLine(size_t line);
  virtual void UpdatePopupAppearance();
  virtual void OnHoverEnabledOrDisabled(bool disabled);
  virtual void PaintUpdatesNow();
  virtual AutocompletePopupModel* GetModel();

 private:
  // Returns the bounds the popup should be created or shown at, in screen
  // coordinates.
  gfx::Rect GetPopupScreenBounds() const;

  scoped_ptr<AutocompletePopupModel> model_;

  AutocompleteEditViewWin* edit_view_;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupWin);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_
