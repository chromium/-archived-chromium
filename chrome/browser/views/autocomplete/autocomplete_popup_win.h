// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_
#define CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_

#include "views/widget/widget_win.h"

class AutocompleteEditViewWin;
class AutocompletePopupContentsView;

class AutocompletePopupWin : public views::WidgetWin {
 public:
  explicit AutocompletePopupWin(AutocompletePopupContentsView* contents);
  virtual ~AutocompletePopupWin();

  // Creates the popup and shows it for the first time. |edit_view| is the edit
  // that created us.
  void Init(AutocompleteEditViewWin* edit_view, views::View* contents);

  // Shows the popup and moves it to the right position.
  void Show();

 protected:
  // Overridden from WidgetWin:
  virtual LRESULT OnMouseActivate(HWND window,
                                  UINT hit_test,
                                  UINT mouse_message);

 private:
  AutocompletePopupContentsView* contents_;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupWin);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_
