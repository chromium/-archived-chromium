// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_manager.h"

#include "base/message_loop.h"
#include "base/thread.h"
#include "chrome/browser/download/download_request_dialog_delegate.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/notification_service.h"

// TabDownloadState ------------------------------------------------------------

DownloadRequestManager::TabDownloadState::TabDownloadState(
    DownloadRequestManager* host,
    NavigationController* controller,
    NavigationController* originating_controller)
    : host_(host),
      controller_(controller),
      status_(DownloadRequestManager::ALLOW_ONE_DOWNLOAD),
      dialog_delegate_(NULL) {
  Source<NavigationController> notification_source(controller);
  registrar_.Add(this, NotificationType::NAV_ENTRY_PENDING,
                 notification_source);
  registrar_.Add(this, NotificationType::TAB_CLOSED, notification_source);

  NavigationEntry* active_entry = originating_controller ?
      originating_controller->GetActiveEntry() : controller->GetActiveEntry();
  if (active_entry)
    initial_page_host_ = active_entry->url().host();
}

DownloadRequestManager::TabDownloadState::~TabDownloadState() {
  // We should only be destroyed after the callbacks have been notified.
  DCHECK(callbacks_.empty());

  // And we should have closed the message box.
  DCHECK(!dialog_delegate_);
}

void DownloadRequestManager::TabDownloadState::OnUserGesture() {
  if (is_showing_prompt()) {
    // Don't change the state if the user clicks on the page some where.
    return;
  }

  if (status_ != DownloadRequestManager::ALLOW_ALL_DOWNLOADS &&
      status_ != DownloadRequestManager::DOWNLOADS_NOT_ALLOWED) {
    // Revert to default status.
    host_->Remove(this);
    // WARNING: We've been deleted.
    return;
  }
}

void DownloadRequestManager::TabDownloadState::PromptUserForDownload(
    TabContents* tab,
    DownloadRequestManager::Callback* callback) {
  callbacks_.push_back(callback);

  if (is_showing_prompt())
    return;  // Already showing prompt.

  if (DownloadRequestManager::delegate_)
    NotifyCallbacks(DownloadRequestManager::delegate_->ShouldAllowDownload());
  else
    dialog_delegate_ = DownloadRequestDialogDelegate::Create(tab, this);
}

void DownloadRequestManager::TabDownloadState::Cancel() {
  NotifyCallbacks(false);
}

void DownloadRequestManager::TabDownloadState::Accept() {
  NotifyCallbacks(true);
}

void DownloadRequestManager::TabDownloadState::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if ((type != NotificationType::NAV_ENTRY_PENDING &&
       type != NotificationType::TAB_CLOSED) ||
      Source<NavigationController>(source).ptr() != controller_) {
    NOTREACHED();
    return;
  }

  switch(type.value) {
    case NotificationType::NAV_ENTRY_PENDING: {
      // NOTE: resetting state on a pending navigate isn't ideal. In particular
      // it is possible that queued up downloads for the page before the
      // pending navigate will be delivered to us after we process this
      // request. If this happens we may let a download through that we
      // shouldn't have. But this is rather rare, and it is difficult to get
      // 100% right, so we don't deal with it.
      NavigationEntry* entry = controller_->GetPendingEntry();
      if (!entry)
        return;

      if (PageTransition::IsRedirect(entry->transition_type())) {
        // Redirects don't count.
        return;
      }

      if (is_showing_prompt()) {
        // We're prompting the user and they navigated away. Close the popup and
        // cancel the downloads.
        dialog_delegate_->CloseWindow();
        // After switch we'll notify callbacks and get deleted.
      } else if (status_ == DownloadRequestManager::ALLOW_ALL_DOWNLOADS ||
                 status_ == DownloadRequestManager::DOWNLOADS_NOT_ALLOWED) {
        // User has either allowed all downloads or canceled all downloads. Only
        // reset the download state if the user is navigating to a different
        // host (or host is empty).
        if (!initial_page_host_.empty() && !entry->url().host().empty() &&
            entry->url().host() == initial_page_host_) {
          return;
        }
      }  // else case: we're not prompting user and user hasn't allowed or
         // disallowed downloads, break so that we get deleted after switch.
      break;
    }

    case NotificationType::TAB_CLOSED:
      // Tab closed, no need to handle closing the dialog as it's owned by the
      // TabContents, break so that we get deleted after switch.
      break;

    default:
      NOTREACHED();
  }

  NotifyCallbacks(false);
  host_->Remove(this);
}

void DownloadRequestManager::TabDownloadState::NotifyCallbacks(bool allow) {
  if (dialog_delegate_) {
    // Reset the delegate so we don't get notified again.
    dialog_delegate_->set_host(NULL);
    dialog_delegate_ = NULL;
  }
  status_ = allow ?
      DownloadRequestManager::ALLOW_ALL_DOWNLOADS :
      DownloadRequestManager::DOWNLOADS_NOT_ALLOWED;
  std::vector<DownloadRequestManager::Callback*> callbacks;
  callbacks.swap(callbacks_);
  for (size_t i = 0; i < callbacks.size(); ++i)
    host_->ScheduleNotification(callbacks[i], allow);
}

namespace {

// DialogDelegateImpl ----------------------------------------------------------

}  // namespace

// DownloadRequestManager ------------------------------------------------------

DownloadRequestManager::DownloadRequestManager(MessageLoop* io_loop,
                                               MessageLoop* ui_loop)
    : io_loop_(io_loop),
      ui_loop_(ui_loop) {
}

DownloadRequestManager::~DownloadRequestManager() {
  // All the tabs should have closed before us, which sends notification and
  // removes from state_map_. As such, there should be no pending callbacks.
  DCHECK(state_map_.empty());
}

DownloadRequestManager::DownloadStatus
    DownloadRequestManager::GetDownloadStatus(TabContents* tab) {
  TabDownloadState* state = GetDownloadState(tab->controller(), NULL, false);
  return state ? state->download_status() : ALLOW_ONE_DOWNLOAD;
}

void DownloadRequestManager::CanDownloadOnIOThread(int render_process_host_id,
                                                   int render_view_id,
                                                   Callback* callback) {
  // This is invoked on the IO thread. Schedule the task to run on the UI
  // thread so that we can query UI state.
  DCHECK(!io_loop_ || io_loop_ == MessageLoop::current());
  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &DownloadRequestManager::CanDownload,
                        render_process_host_id, render_view_id, callback));
}

void DownloadRequestManager::OnUserGesture(TabContents* tab) {
  NavigationController* controller = tab->controller();
  if (!controller) {
    NOTREACHED();
    return;
  }

  TabDownloadState* state = GetDownloadState(controller, NULL, false);
  if (!state)
    return;

  state->OnUserGesture();
}

// static
void DownloadRequestManager::SetTestingDelegate(TestingDelegate* delegate) {
  delegate_ = delegate;
}

DownloadRequestManager::TabDownloadState* DownloadRequestManager::
    GetDownloadState(NavigationController* controller,
                     NavigationController* originating_controller,
                     bool create) {
  DCHECK(controller);
  StateMap::iterator i = state_map_.find(controller);
  if (i != state_map_.end())
    return i->second;

  if (!create)
    return NULL;

  TabDownloadState* state =
      new TabDownloadState(this, controller, originating_controller);
  state_map_[controller] = state;
  return state;
}

void DownloadRequestManager::CanDownload(int render_process_host_id,
                                         int render_view_id,
                                         Callback* callback) {
  DCHECK(!ui_loop_ || MessageLoop::current() == ui_loop_);

  WebContents* originating_tab =
      tab_util::GetWebContentsByID(render_process_host_id, render_view_id);
  if (!originating_tab) {
    // The tab was closed, don't allow the download.
    ScheduleNotification(callback, false);
    return;
  }
  CanDownloadImpl(originating_tab, callback);
}

void DownloadRequestManager::CanDownloadImpl(
    TabContents* originating_tab,
    Callback* callback) {
  TabContents* effective_tab = originating_tab;
  if (effective_tab->delegate() &&
      effective_tab->delegate()->GetConstrainingContents(effective_tab)) {
    // The tab requesting the download is a constrained popup that is not
    // shown, treat the request as if it came from the parent.
    effective_tab =
        effective_tab->delegate()->GetConstrainingContents(effective_tab);
  }

  NavigationController* controller = effective_tab->controller();
  DCHECK(controller);
  TabDownloadState* state = GetDownloadState(
      controller, originating_tab->controller(), true);
  switch (state->download_status()) {
    case ALLOW_ALL_DOWNLOADS:
      ScheduleNotification(callback, true);
      break;

    case ALLOW_ONE_DOWNLOAD:
      state->set_download_status(PROMPT_BEFORE_DOWNLOAD);
      ScheduleNotification(callback, true);
      break;

    case DOWNLOADS_NOT_ALLOWED:
      ScheduleNotification(callback, false);
      break;

    case PROMPT_BEFORE_DOWNLOAD:
      state->PromptUserForDownload(effective_tab, callback);
      break;

    default:
      NOTREACHED();
  }
}

void DownloadRequestManager::ScheduleNotification(Callback* callback,
                                                  bool allow) {
  if (io_loop_) {
    io_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DownloadRequestManager::NotifyCallback,
                          callback, allow));
  } else {
    NotifyCallback(callback, allow);
  }
}

void DownloadRequestManager::NotifyCallback(Callback* callback, bool allow) {
  // We better be on the IO thread now.
  DCHECK(!io_loop_ || MessageLoop::current() == io_loop_);
  if (allow)
    callback->ContinueDownload();
  else
    callback->CancelDownload();
}

void DownloadRequestManager::Remove(TabDownloadState* state) {
  DCHECK(state_map_.find(state->controller()) != state_map_.end());
  state_map_.erase(state->controller());
  delete state;
}

// static
DownloadRequestManager::TestingDelegate* DownloadRequestManager::delegate_ =
    NULL;
