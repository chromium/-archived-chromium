// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/find_bar_controller.h"

#include "build/build_config.h"
#include "chrome/browser/find_bar.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/notification_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"

FindBarController::FindBarController(FindBar* find_bar)
    : find_bar_(find_bar),
      tab_contents_(NULL),
      last_reported_matchcount_(0) {
}

FindBarController::~FindBarController() {
  DCHECK(!tab_contents_);
}

void FindBarController::Show() {
  // Only show the animation if we're not already showing a find bar for the
  // selected TabContents.
  if (!tab_contents_->find_ui_active()) {
    tab_contents_->set_find_ui_active(true);
    find_bar_->Show();
  }
  find_bar_->SetFocusAndSelection();
}

void FindBarController::EndFindSession() {
  find_bar_->Hide(true);

  // |tab_contents_| can be NULL for a number of reasons, for example when the
  // tab is closing. We must guard against that case. See issue 8030.
  if (tab_contents_) {
    // When we hide the window, we need to notify the renderer that we are done
    // for now, so that we can abort the scoping effort and clear all the
    // tickmarks and highlighting.
    tab_contents_->StopFinding(false);  // false = don't clear selection on
                                        // page.
    find_bar_->ClearResults(tab_contents_->find_result());

    // When we get dismissed we restore the focus to where it belongs.
    find_bar_->RestoreSavedFocus();
  }
}

void FindBarController::ChangeTabContents(TabContents* contents) {
  if (tab_contents_) {
    registrar_.RemoveAll();
    find_bar_->StopAnimation();
  }

  tab_contents_ = contents;

  // Hide any visible find window from the previous tab if NULL |tab_contents|
  // is passed in or if the find UI is not active in the new tab.
  if (find_bar_->IsFindBarVisible() &&
      (!tab_contents_ || !tab_contents_->find_ui_active())) {
    find_bar_->Hide(false);
  }

  if (!tab_contents_)
    return;

  registrar_.Add(this, NotificationType::FIND_RESULT_AVAILABLE,
                 Source<TabContents>(tab_contents_));
  registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(&tab_contents_->controller()));

  // Find out what we should show in the find text box. Usually, this will be
  // the last search in this tab, but if no search has been issued in this tab
  // we use the last search string (from any tab).
  string16 find_string = tab_contents_->find_text();
  if (find_string.empty())
    find_string = tab_contents_->find_prepopulate_text();

  // Update the find bar with existing results and search text, regardless of
  // whether or not the find bar is visible, so that if it's subsequently
  // shown it is showing the right state for this tab. We update the find text
  // _first_ since the FindBarView checks its emptiness to see if it should
  // clear the result count display when there's nothing in the box.
  find_bar_->SetFindText(find_string);

  if (tab_contents_->find_ui_active()) {
    // A tab with a visible find bar just got selected and we need to show the
    // find bar but without animation since it was already animated into its
    // visible state. We also want to reset the window location so that
    // we don't surprise the user by popping up to the left for no apparent
    // reason.
    gfx::Rect new_pos = find_bar_->GetDialogPosition(gfx::Rect());
    find_bar_->SetDialogPosition(new_pos, false);

    // Only modify focus and selection if Find is active, otherwise the Find
    // Bar will interfere with user input.
    find_bar_->SetFocusAndSelection();
  }

  UpdateFindBarForCurrentResult();
}

////////////////////////////////////////////////////////////////////////////////
// FindBarWin, NotificationObserver implementation:

void FindBarController::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  if (type == NotificationType::FIND_RESULT_AVAILABLE) {
    // Don't update for notifications from TabContentses other than the one we
    // are actively tracking.
    if (Source<TabContents>(source).ptr() == tab_contents_) {
      UpdateFindBarForCurrentResult();
      if (tab_contents_->find_result().final_update() &&
          tab_contents_->find_result().number_of_matches() == 0) {
        find_bar_->AudibleAlert();
      }
    }
  } else if (type == NotificationType::NAV_ENTRY_COMMITTED) {
    NavigationController* source_controller =
        Source<NavigationController>(source).ptr();
    if (source_controller == &tab_contents_->controller()) {
      NavigationController::LoadCommittedDetails* commit_details =
          Details<NavigationController::LoadCommittedDetails>(details).ptr();
      PageTransition::Type transition_type =
          commit_details->entry->transition_type();
      // We hide the FindInPage window when the user navigates away, except on
      // reload.
      if (find_bar_->IsFindBarVisible()) {
        if (PageTransition::StripQualifier(transition_type) !=
            PageTransition::RELOAD) {
          EndFindSession();
        } else {
          // On Reload we want to make sure FindNext is converted to a full Find
          // to make sure highlights for inactive matches are repainted.
          tab_contents_->set_find_op_aborted(true);
        }
      }
    }
  }
}

void FindBarController::UpdateFindBarForCurrentResult() {
  const FindNotificationDetails& find_result = tab_contents_->find_result();

  // Avoid bug 894389: When a new search starts (and finds something) it reports
  // an interim match count result of 1 before the scoping effort starts. This
  // is to provide feedback as early as possible that we will find something.
  // As you add letters to the search term, this creates a flashing effect when
  // we briefly show "1 of 1" matches because there is a slight delay until
  // the scoping effort starts updating the match count. We avoid this flash by
  // ignoring interim results of 1 if we already have a positive number.
  if (find_result.number_of_matches() > -1) {
    if (last_reported_matchcount_ > 0 &&
        find_result.number_of_matches() == 1 &&
        !find_result.final_update())
      return;  // Don't let interim result override match count.
    last_reported_matchcount_ = find_result.number_of_matches();
  }

  find_bar_->UpdateUIForFindResult(find_result, tab_contents_->find_text());
}
