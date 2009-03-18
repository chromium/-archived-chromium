// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/find_bar_controller.h"

#include "build/build_config.h"
#include "chrome/browser/find_bar.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/notification_service.h"
#include "chrome/browser/tab_contents/web_contents.h"

FindBarController::FindBarController(FindBar* find_bar)
    : find_bar_(find_bar), web_contents_(NULL) {
}

FindBarController::~FindBarController() {
  // Web contents should have been NULLed out. If not, then we're leaking
  // notification observers.
  DCHECK(!web_contents_);
}

void FindBarController::Show() {
  // Only show the animation if we're not already showing a find bar for the
  // selected WebContents.
  if (!web_contents_->find_ui_active()) {
    web_contents_->set_find_ui_active(true);
    find_bar_->Show();
  }
  find_bar_->SetFocusAndSelection();
}

void FindBarController::EndFindSession() {
  find_bar_->Hide(true);

  // |web_contents_| can be NULL for a number of reasons, for example when the
  // tab is closing. We must guard against that case. See issue 8030.
  if (web_contents_) {
    // When we hide the window, we need to notify the renderer that we are done
    // for now, so that we can abort the scoping effort and clear all the
    // tickmarks and highlighting.
    web_contents_->StopFinding(false);  // false = don't clear selection on
                                        // page.
    find_bar_->ClearResults(web_contents_->find_result());

    // When we get dismissed we restore the focus to where it belongs.
    find_bar_->RestoreSavedFocus();
  }
}

void FindBarController::ChangeWebContents(WebContents* contents) {
  if (web_contents_) {
    NotificationService::current()->RemoveObserver(
        this, NotificationType::FIND_RESULT_AVAILABLE,
        Source<TabContents>(web_contents_));
    NotificationService::current()->RemoveObserver(
        this, NotificationType::NAV_ENTRY_COMMITTED,
        Source<NavigationController>(web_contents_->controller()));
    find_bar_->StopAnimation();
  }

  web_contents_ = contents;

  // Hide any visible find window from the previous tab if NULL |web_contents|
  // is passed in or if the find UI is not active in the new tab.
  if (find_bar_->IsFindBarVisible() &&
      (!web_contents_ || !web_contents_->find_ui_active())) {
    find_bar_->Hide(false);
  }

  if (web_contents_) {
    NotificationService::current()->AddObserver(
        this, NotificationType::FIND_RESULT_AVAILABLE,
        Source<TabContents>(web_contents_));
    NotificationService::current()->AddObserver(
        this, NotificationType::NAV_ENTRY_COMMITTED,
        Source<NavigationController>(web_contents_->controller()));

    // Update the find bar with existing results and search text, regardless of
    // whether or not the find bar is visible, so that if it's subsequently
    // shown it is showing the right state for this tab. We update the find text
    // _first_ since the FindBarView checks its emptiness to see if it should
    // clear the result count display when there's nothing in the box.
    find_bar_->SetFindText(web_contents_->find_text());

    if (web_contents_->find_ui_active()) {
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

    find_bar_->UpdateUIForFindResult(web_contents_->find_result(),
                                     web_contents_->find_text());
  }
}

////////////////////////////////////////////////////////////////////////////////
// FindBarWin, NotificationObserver implementation:

void FindBarController::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  if (type == NotificationType::FIND_RESULT_AVAILABLE) {
    // Don't update for notifications from TabContentses other than the one we
    // are actively tracking.
    if (Source<TabContents>(source).ptr() == web_contents_) {
      find_bar_->UpdateUIForFindResult(web_contents_->find_result(),
                                       web_contents_->find_text());
    }
  } else if (type == NotificationType::NAV_ENTRY_COMMITTED) {
    NavigationController* source_controller =
        Source<NavigationController>(source).ptr();
    if (source_controller == web_contents_->controller()) {
      NavigationController::LoadCommittedDetails* commit_details =
          Details<NavigationController::LoadCommittedDetails>(details).ptr();
      PageTransition::Type transition_type =
          commit_details->entry->transition_type();
      // We hide the FindInPage window when the user navigates away, except on
      // reload.
      if (find_bar_->IsFindBarVisible() &&
          PageTransition::StripQualifier(transition_type) !=
              PageTransition::RELOAD) {
        EndFindSession();
      }
    }
  }
}
