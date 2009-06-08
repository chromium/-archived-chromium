// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CONTAINER_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CONTAINER_H_

#include "views/view.h"

class TabOverviewGrid;

// TabOverviewContainer contains TabOverviewGrid. TabOverviewContainer provides
// padding around the grid as well as maintaining a shape on the containing
// widget.
class TabOverviewContainer : public views::View {
 public:
  TabOverviewContainer();
  virtual ~TabOverviewContainer();

  // Sets the max size. This ends up being passed down to the grid after
  // adjusting for our borders.
  void SetMaxSize(const gfx::Size& max_size);

  // Updates the shape on the containing widget.
  void UpdateWidgetShape(int width, int height);

  // View overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);

 private:
  TabOverviewGrid* GetTabOverviewGrid();

  DISALLOW_COPY_AND_ASSIGN(TabOverviewContainer);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CONTAINER_H_
