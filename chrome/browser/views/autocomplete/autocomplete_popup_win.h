// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_
#define CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_

#include "chrome/browser/autocomplete/autocomplete_popup_view.h"
#include "chrome/views/widget/widget_win.h"

class AutocompleteEditModel;
class AutocompleteEditViewWin;
class AutocompletePopupViewContents;
class Profile;

// Interface to retrieve the position of the popup.
class AutocompletePopupPositioner {
 public:
  // Returns the bounds at which the popup should be shown, in screen
  // coordinates. The height is ignored, since the popup is sized to its
  // contents automatically.
  virtual gfx::Rect GetPopupBounds() const = 0;
};

class AutocompletePopupWin : public views::WidgetWin,
                             public AutocompletePopupView {
 public:
  AutocompletePopupWin(const ChromeFont& font,
                       AutocompleteEditViewWin* edit_view,
                       AutocompleteEditModel* edit_model,
                       Profile* profile,
                       AutocompletePopupPositioner* popup_positioner);
  virtual ~AutocompletePopupWin();

  // Overridden from AutocompletePopupView:
  virtual bool IsOpen() const;
  virtual void InvalidateLine(size_t line);
  virtual void UpdatePopupAppearance();
  virtual void OnHoverEnabledOrDisabled(bool disabled);
  virtual void PaintUpdatesNow();
  virtual AutocompletePopupModel* GetModel();

 private:
  // Returns the bounds of the popup window, in screen coordinates, adjusted for
  // the amount of drop shadow the contents view may wish to add.
  gfx::Rect GetPopupBounds() const;

  // The provider of our result set.
  scoped_ptr<AutocompletePopupModel> model_;

  // The edit view that invokes us.
  AutocompleteEditViewWin* edit_view_;

  // An object that tells the popup how to position itself.
  AutocompletePopupPositioner* popup_positioner_;

  // The view that holds the result views.
  AutocompletePopupViewContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupWin);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_
