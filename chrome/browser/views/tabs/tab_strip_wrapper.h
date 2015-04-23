// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_STRIP_WRAPPER_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_STRIP_WRAPPER_H_

class BrowserTabStrip;
namespace gfx {
class Point;
class Rect;
}
class TabStrip;
class TabStripModel;
namespace views {
class View;
}

// A temporary interface to abstract the TabStrip implementation (which can be
// either TabStrip or TabStrip2) from the rest of the Browser frontend code
// while the new TabStrip is brought up.
class TabStripWrapper {
 public:
  // Returns the preferred height of this TabStrip. This is based on the
  // typical height of its constituent tabs.
  virtual int GetPreferredHeight() = 0;

  // Returns true if Tabs in this TabStrip are currently changing size or
  // position.
  virtual bool IsAnimating() const = 0;

  // Set the background offset used by inactive tabs to match the frame image.
  virtual void SetBackgroundOffset(gfx::Point offset) = 0;

  // Returns true if the specified point(TabStrip coordinates) should be
  // considered to be within the window caption area of the browser window.
  virtual bool PointIsWithinWindowCaption(const gfx::Point& point) = 0;

  // Returns true if a drag session is currently active.
  virtual bool IsDragSessionActive() const = 0;

  // Return true if this tab strip is compatible with the provided tab strip.
  // Compatible tab strips can transfer tabs during drag and drop.
  virtual bool IsCompatibleWith(TabStripWrapper* other) const = 0;

  // Sets the bounds of the tab at the specified |tab_index|. |tab_bounds| are
  // in TabStrip coordinates.
  virtual void SetDraggedTabBounds(int tab_index,
                                   const gfx::Rect& tab_bounds) = 0;

  // Updates the loading animations displayed by tabs in the tabstrip to the
  // next frame.
  virtual void UpdateLoadingAnimations() = 0;

  // Returns the views::View of the wrapped tabstrip, for layout and sizing.
  virtual views::View* GetView() = 0;

  // Shim to provide access to the BrowserTabStrip implementation for code only
  // called from within TabStrip2::Enabled() == true blocks. Returns NULL when
  // old TabStrip is in effect.
  virtual BrowserTabStrip* AsBrowserTabStrip() = 0;

  // Shim to provide access to the TabStrip implementation for code only called
  // from within TabStrip2::Enabled() == false blocks. Returns NULL when the new
  // TabStrip is in effect.
  virtual TabStrip* AsTabStrip() = 0;

  // Creates a TabStrip - either the old or new one depending on command line
  // flags.
  static TabStripWrapper* CreateTabStrip(TabStripModel* model);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_STRIP_WRAPPER_H_
