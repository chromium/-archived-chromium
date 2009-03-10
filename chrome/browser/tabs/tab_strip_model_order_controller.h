// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TABS_TAB_STRIP_MODEL_ORDER_CONTROLLER_H__
#define CHROME_BROWSER_TABS_TAB_STRIP_MODEL_ORDER_CONTROLLER_H__

#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/pref_member.h"

class TabContents;
class TabStripModel;

///////////////////////////////////////////////////////////////////////////////
// TabStripModelOrderController
//
//  An object that allows different types of ordering and reselection to be
//  heuristics plugged into a TabStripModel.
//
class TabStripModelOrderController : public TabStripModelObserver {
 public:
  explicit TabStripModelOrderController(TabStripModel* tabstrip);
  virtual ~TabStripModelOrderController();

  // Determine where to place a newly opened tab by using the supplied
  // transition and foreground flag to figure out how it was opened.
  int DetermineInsertionIndex(TabContents* new_contents,
                              PageTransition::Type transition,
                              bool foreground);

  // Determine where to shift selection after a tab is closed.
  int DetermineNewSelectedIndex(int removed_index) const;

  // Overridden from TabStripModelObserver:
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture);

 protected:
  // Returns a valid index to be selected after the tab at |removing_index| is
  // closed. If |index| is after |removing_index|, |index| is adjusted to
  // reflect the fact that |removing_index| is going away.
  int GetValidIndex(int index, int removing_index) const;

  TabStripModel* tabstrip_;
};

#endif  // CHROME_BROWSER_TABS_TAB_STRIP_MODEL_ORDER_CONTROLLER_H__
