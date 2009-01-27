// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tabs/tab_strip_model_order_controller.h"

#if defined(OS_WIN)
#include "chrome/browser/tab_contents/tab_contents.h"
#elif defined(OS_MACOSX) || (OS_LINUX)
// TODO(port): remove this when the mock of TabContents is removed
#include "chrome/common/temp_scaffolding_stubs.h"
#endif
#include "chrome/common/pref_names.h"

///////////////////////////////////////////////////////////////////////////////
// TabStripModelOrderController, public:

TabStripModelOrderController::TabStripModelOrderController(
    TabStripModel* tabstrip) : tabstrip_(tabstrip) {
  tabstrip_->AddObserver(this);
}

TabStripModelOrderController::~TabStripModelOrderController() {
  tabstrip_->RemoveObserver(this);
}

int TabStripModelOrderController::DetermineInsertionIndex(
    TabContents* new_contents,
    PageTransition::Type transition,
    bool foreground) {
  int tab_count = tabstrip_->count();
  if (!tab_count)
    return 0;

  if (transition == PageTransition::LINK && tabstrip_->selected_index() != -1) {
    if (foreground) {
      // If the page was opened in the foreground by a link click in another tab,
      // insert it adjacent to the tab that opened that link.
      // TODO(beng): (http://b/1085481) may want to open right of all locked
      //             tabs?
      return tabstrip_->selected_index() + 1;
    }
    NavigationController* opener =
        tabstrip_->GetSelectedTabContents()->controller();
    // Get the index of the next item opened by this tab, and insert before
    // it...
    int index = tabstrip_->GetIndexOfLastTabContentsOpenedBy(
        opener, tabstrip_->selected_index());
    if (index != TabStripModel::kNoTab)
      return index + 1;
    // Otherwise insert adjacent to opener...
    return tabstrip_->selected_index() + 1;
  }
  // In other cases, such as Ctrl+T, open at the end of the strip.
  return tab_count;
}

int TabStripModelOrderController::DetermineNewSelectedIndex(
    int removing_index) const {
  int tab_count = tabstrip_->count();
  DCHECK(removing_index >= 0 && removing_index < tab_count);
  NavigationController* parent_opener =
      tabstrip_->GetOpenerOfTabContentsAt(removing_index);
  // First see if the index being removed has any "child" tabs. If it does, we
  // want to select the first in that child group, not the next tab in the same
  // group of the removed tab.
  NavigationController* removed_controller =
      tabstrip_->GetTabContentsAt(removing_index)->controller();
  int index = tabstrip_->GetIndexOfNextTabContentsOpenedBy(removed_controller,
                                                           removing_index,
                                                           false);
  if (index != TabStripModel::kNoTab)
    return GetValidIndex(index, removing_index);

  if (parent_opener) {
    // If the tab was in a group, shift selection to the next tab in the group.
    int index = tabstrip_->GetIndexOfNextTabContentsOpenedBy(parent_opener,
                                                             removing_index,
                                                             false);
    if (index != TabStripModel::kNoTab)
      return GetValidIndex(index, removing_index);

    // If we can't find a subsequent group member, just fall back to the
    // parent_opener itself. Note that we use "group" here since opener is
    // reset by select operations..
    index = tabstrip_->GetIndexOfController(parent_opener);
    if (index != TabStripModel::kNoTab)
      return GetValidIndex(index, removing_index);
  }

  // No opener set, fall through to the default handler...
  int selected_index = tabstrip_->selected_index();
  if (selected_index >= (tab_count - 1))
    return selected_index - 1;
  return selected_index;
}

void TabStripModelOrderController::TabSelectedAt(TabContents* old_contents,
                                                 TabContents* new_contents,
                                                 int index,
                                                 bool user_gesture) {
  NavigationController* old_opener = NULL;
  if (old_contents) {
    int index = tabstrip_->GetIndexOfTabContents(old_contents);
    if (index != TabStripModel::kNoTab) {
      old_opener = tabstrip_->GetOpenerOfTabContentsAt(index);

      // Forget any group/opener relationships that need to be reset whenever
      // selection changes (see comment in TabStripModel::AddTabContentsAt).
      if (tabstrip_->ShouldResetGroupOnSelect(old_contents))
        tabstrip_->ForgetGroup(old_contents);
    }
  }
  NavigationController* new_opener =
      tabstrip_->GetOpenerOfTabContentsAt(index);
  if (user_gesture && new_opener != old_opener &&
      new_opener != old_contents->controller() &&
      old_opener != new_contents->controller()) {
    tabstrip_->ForgetAllOpeners();
  }
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModelOrderController, private:

int TabStripModelOrderController::GetValidIndex(int index,
                                                int removing_index) const {
  if (removing_index < index)
    index = std::max(0, index - 1);
  return index;
}

