// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CONTROLLER_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CONTROLLER_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/views/tabs/tab_overview_grid.h"

class TabOverviewContainer;
class TabStripModel;

namespace views {
class Widget;
}
namespace gfx {
class Point;
}

// TabOverviewController is responsible for showing a TabOverviewGrid.
class TabOverviewController : public TabOverviewGrid::Host {
 public:
  // Creates a TabOverviewController that will be shown on the monitor
  // containing |monitor_origin|.
  explicit TabOverviewController(const gfx::Point& monitor_origin);
  ~TabOverviewController();

  // Sets the tarb strip to show.
  void SetTabStripModel(TabStripModel* tab_strip_model);

  // Shows/hides the grid.
  void Show();
  void Hide();

  // TabOverviewGrid::Host overrides.
  virtual void TabOverviewGridPreferredSizeChanged();
  virtual void SelectTabContents(TabContents* contents);

 private:
  // The widget showing the view.
  scoped_ptr<views::Widget> host_;

  // Bounds of the monitor we're being displayed on. This is used to position
  // the widget.
  gfx::Rect monitor_bounds_;

  // View containing the grid, owned by host.
  TabOverviewContainer* container_;

  // The view. This is owned by host.
  TabOverviewGrid* view_;

  // The model, not owned by us.
  TabStripModel* model_;

  DISALLOW_COPY_AND_ASSIGN(TabOverviewController);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CONTROLLER_H_
