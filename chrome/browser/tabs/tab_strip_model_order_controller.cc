// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/tabs/tab_strip_model_order_controller.h"

#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
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
