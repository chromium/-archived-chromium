// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
#define CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_

#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/views/view.h"

class AutocompletePopupWin;

// TODO(beng): documentation, finalize
class AutocompleteResultViewModel {
 public:
  virtual bool IsSelectedIndex(size_t index) = 0;

  virtual AutocompleteMatch::Type GetResultTypeAtIndex(size_t index) = 0;

  virtual void OpenIndex(size_t index, WindowOpenDisposition disposition) = 0;

  virtual void SetHoveredLine(size_t index) = 0;
  virtual void SetSelectedLine(size_t index, bool revert_to_default) = 0;
};

// TODO(beng): documentation
class AutocompletePopupContentsView : public views::View,
                                      public AutocompleteResultViewModel {
 public:
  explicit AutocompletePopupContentsView(AutocompletePopupWin* popup);
  virtual ~AutocompletePopupContentsView() {}

  // Update the presentation with the latest result.
  void SetAutocompleteResult(const AutocompleteResult& result);

  // Schedule a repaint for the specified row.
  void InvalidateLine(int index);

  // Overridden from AutocompleteResultViewModel:
  virtual bool IsSelectedIndex(size_t index);
  virtual AutocompleteMatch::Type GetResultTypeAtIndex(size_t index);
  virtual void OpenIndex(size_t index, WindowOpenDisposition disposition);
  virtual void SetHoveredLine(size_t index);
  virtual void SetSelectedLine(size_t index, bool revert_to_default);

  // Overridden from views::View:
  virtual void PaintChildren(ChromeCanvas* canvas);
  virtual void Layout();

 private:
  // Fill a path for the contents' roundrect. |bounding_rect| is the rect that
  // bounds the path.
  void MakeContentsPath(gfx::Path* path, const gfx::Rect& bounding_rect);

  // Updates the window's blur region for the current size.
  void UpdateBlurRegion();

  // Makes the contents of the canvas slightly transparent.
  void MakeCanvasTransparent(ChromeCanvas* canvas);

  AutocompletePopupWin* popup_;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupContentsView);
};


#endif  // #ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
