// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEW_TAB_ICON_VIEW_H__
#define CHROME_BROWSER_VIEW_TAB_ICON_VIEW_H__

#include "chrome/views/view.h"

class TabContents;

////////////////////////////////////////////////////////////////////////////////
//
// A view to display a tab fav icon or a throbber.
//
////////////////////////////////////////////////////////////////////////////////
class TabIconView : public views::View {
 public:
  class TabContentsProvider {
   public:
    // Should return the current tab contents this TabIconView object is
    // representing.
    virtual TabContents* GetCurrentTabContents() = 0;

    // Returns the favicon to display in the icon view
    virtual SkBitmap GetFavIcon() = 0;
  };

  static void InitializeIfNeeded();

  explicit TabIconView(TabContentsProvider* provider);
  virtual ~TabIconView();

  // Invoke whenever the tab state changes or the throbber should update.
  void Update();

  // Set the throbber to the light style (for use on dark backgrounds).
  void set_is_light(bool is_light) { is_light_ = is_light; }

  // Overriden from View
  virtual void Paint(ChromeCanvas* canvas);
  virtual gfx::Size GetPreferredSize();

 private:
  void PaintThrobber(ChromeCanvas* canvas);
  void PaintFavIcon(ChromeCanvas* canvas, const SkBitmap& bitmap);

  // Our provider of current tab contents.
  TabContentsProvider* provider_;

  // Whether the throbber is running.
  bool throbber_running_;

  // Whether we should display our light or dark style.
  bool is_light_;

  // Current frame of the throbber being painted. This is only used if
  // throbber_running_ is true.
  int throbber_frame_;

  DISALLOW_EVIL_CONSTRUCTORS(TabIconView);
};

#endif  // CHROME_BROWSER_VIEW_TAB_ICON_VIEW_H__

