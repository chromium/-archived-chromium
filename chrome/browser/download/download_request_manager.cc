// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_manager.h"

#include "base/message_loop.h"
#include "base/thread.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/constrained_window.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/browser/tab_util.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/message_box_view.h"

#include "generated_resources.h"

namespace {

// DialogDelegateImpl ----------------------------------------------------------

// DialogDelegateImpl is the DialogDelegate implementation used to prompt the
// the user as to whether they want to allow multiple downloads.
// DialogDelegateImpl delegates the allow/cancel methods to the
// TabDownloadState.
//
// TabDownloadState does not directly implement DialogDelegate, rather it is
// split into DialogDelegateImpl as TabDownloadState may be deleted before
// the dialog.

class DialogDelegateImpl : public views::DialogDelegate {
 public:
  DialogDelegateImpl(TabContents* tab,
                     DownloadRequestManager::TabDownloadState* host);

  void set_host(DownloadRequestManager::TabDownloadState* host) {
    host_ = host;
  }

  // Closes the prompt.
  void CloseWindow();

 private:
  // DialogDelegate methods;
  virtual bool Cancel();
  virtual bool Accept();
  virtual views::View* GetContentsView() { return message_view_; }
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual int GetDefaultDialogButton() const {
    return DIALOGBUTTON_CANCEL;
  }
  virtual void WindowClosing();

  // The TabDownloadState we're displaying the dialog for. May be null.
  DownloadRequestManager::TabDownloadState* host_;

  MessageBoxView* message_view_;

  ConstrainedWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(DialogDelegateImpl);
};

}  // namespace

// TabDownloadState ------------------------------------------------------------

// TabDownloadState maintains the download state for a particular tab.
// TabDownloadState installs observers to update the download status
// appropriately. Additionally TabDownloadState prompts the user as necessary.
// TabDownloadState deletes itself (by invoking DownloadRequestManager::Remove)
// as necessary.

class DownloadRequestManager::TabDownloadState : public NotificationObserver {
 public:
  // Creates a new TabDownloadState. |controller| is the controller the
  // TabDownloadState tracks the state of and is the host for any dialogs that
  // are displayed. |originating_controller| is used to determine the host of
  // the initial download. If |originating_controller| is null, |controller| is
  // used. |originating_controller| is typically null, but differs from
  // |controller| in the case of a constrained popup requesting the download.
  TabDownloadState(DownloadRequestManager* host,
                   NavigationController* controller,
                   NavigationController* originating_controller);
  ~TabDownloadState();

  // Status of the download.
  void set_download_status(DownloadRequestManager::DownloadStatus status) {
    status_ = status;
  }
  DownloadRequestManager::DownloadStatus download_status() const {
    return status_;
  }

  // Invoked when a user gesture occurs (mouse click, enter or space). This
  // may result in invoking Remove on DownloadRequestManager.
  void OnUserGesture();

  // Asks the user if they really want to allow the download.
  // See description above CanDownloadOnIOThread for details on lifetime of
  // callback.
  void PromptUserForDownload(TabContents* tab,
                             DownloadRequestManager::Callback* callback);

  // Are we showing a prompt to the user?
  bool is_showing_prompt() const { return (dialog_delegate_ != NULL); }

  // NavigationController we're tracking.
  NavigationController* controller() const { return controller_; }

  // Invoked from DialogDelegateImpl. Notifies the delegates and changes the
  // status appropriately.
  void Cancel();
  void Accept();

 private:
  // NotificationObserver method.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // Notifies the callbacks as to whether the download is allowed or not.
  // Updates status_ appropriately.
  void NotifyCallbacks(bool allow);

  DownloadRequestManager* host_;

  NavigationController* controller_;

  // Host of the first page the download started on. This may be empty.
  std::string initial_page_host_;

  DownloadRequestManager::DownloadStatus status_;

  // Callbacks we need to notify. This is only non-empty if we're showing a
  // dialog.
  // See description above CanDownloadOnIOThread for details on lifetime of
  // callbacks.
  std::vector<DownloadRequestManager::Callback*> callbacks_;

  // Used to remove observers installed on NavigationController.
  NotificationRegistrar registrar_;

  // Handles showing the dialog to the user, may be null.
  DialogDelegateImpl* dialog_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TabDownloadState);
};

DownloadRequestManager::TabDownloadState::TabDownloadState(
    DownloadRequestManager* host,
    NavigationController* controller,
    NavigationController* originating_controller)
    : host_(host),
      controller_(controller),
      status_(DownloadRequestManager::ALLOW_ONE_DOWNLOAD),
      dialog_delegate_(NULL) {
  Source<NavigationController> notification_source(controller);
  registrar_.Add(this, NOTIFY_NAV_ENTRY_PENDING, notification_source);
  registrar_.Add(this, NOTIFY_TAB_CLOSED, notification_source);

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
    dialog_delegate_ = new DialogDelegateImpl(tab, this);
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
  if ((type != NOTIFY_NAV_ENTRY_PENDING && type != NOTIFY_TAB_CLOSED) ||
       Source<NavigationController>(source).ptr() != controller_) {
    NOTREACHED();
    return;
  }

  switch(type) {
    case NOTIFY_NAV_ENTRY_PENDING: {
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

    case NOTIFY_TAB_CLOSED:
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

DialogDelegateImpl::DialogDelegateImpl(
    TabContents* tab,
    DownloadRequestManager::TabDownloadState* host)
    : host_(host) {
  message_view_ = new MessageBoxView(
      MessageBoxView::kIsConfirmMessageBox,
      l10n_util::GetString(IDS_MULTI_DOWNLOAD_WARNING),
      std::wstring());
  window_ = tab->CreateConstrainedDialog(this, message_view_);
}

void DialogDelegateImpl::CloseWindow() {
  window_->CloseConstrainedWindow();
}

bool DialogDelegateImpl::Cancel() {
  if (host_)
    host_->Cancel();
  return true;
}

bool DialogDelegateImpl::Accept() {
  if (host_)
    host_->Accept();
  return true;
}

std::wstring DialogDelegateImpl::GetDialogButtonLabel(
    DialogButton button) const {
  if (button == DIALOGBUTTON_OK)
    return l10n_util::GetString(IDS_MULTI_DOWNLOAD_WARNING_ALLOW);
  if (button == DIALOGBUTTON_CANCEL)
    return l10n_util::GetString(IDS_MULTI_DOWNLOAD_WARNING_DENY);
  return std::wstring();
}

void DialogDelegateImpl::WindowClosing() {
  DCHECK(!host_);
  delete this;
}

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
