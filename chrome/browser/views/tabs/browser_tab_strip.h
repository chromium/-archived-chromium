// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_BROWSER_TAB_STRIP_H_
#define CHROME_BROWSER_VIEWS_TABS_BROWSER_TAB_STRIP_H_

#include "chrome/browser/views/tabs/tab_strip_2.h"
#include "chrome/browser/tabs/tab_strip_model.h"

// A specialization fo TabStrip2 for the browser window.
//
// TODO(beng): This shouldn't be a subclass of TabStrip2, rather it should own
//             one.
class BrowserTabStrip : public TabStrip2,
                        public TabStrip2Model,
                        public TabStripModelObserver {
 public:
  explicit BrowserTabStrip(TabStripModel* model);
  virtual ~BrowserTabStrip();

  // Detaches the tab at the specified index.
  TabContents* DetachTab(int index);

  // Attaches the specified TabContents at the appropriate position given the
  // mouse cursor at the specified screen position.
  void AttachTab(TabContents* contents, const gfx::Point& screen_point,
                 const gfx::Rect& tab_screen_bounds);

  // Overridden from TabStripModelObserver:
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* contents,
                             int index,
                             bool user_gesture);
  virtual void TabMoved(TabContents* contents, int from_index, int to_index);
  virtual void TabChangedAt(TabContents* contents, int index);

  // Overridden from TabStrip2Model:
  virtual string16 GetTitle(int index) const;
  virtual bool IsSelected(int index) const;
  virtual void SelectTabAt(int index);
  virtual bool CanDragTabs() const;
  virtual void MoveTabAt(int index, int to_index);
  virtual void DetachTabAt(int index,
                           const gfx::Rect& window_bounds,
                           const gfx::Rect& tab_bounds);

 private:
  TabStripModel* model_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTabStrip);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_TABS_BROWSER_TAB_STRIP_H_
