// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_MANAGER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_MANAGER_H_

#include <map>
#include <vector>

#include "base/ref_counted.h"

class MessageLoop;
class NavigationController;
class TabContents;

// DownloadRequestManager is responsible for determining whether a download
// should be allowed or not. It is designed to keep pages from downloading
// multiple files without user interaction. DownloadRequestManager is invoked
// from ResourceDispatcherHost any time a download begins
// (CanDownloadOnIOThread). The request is processed on the UI thread, and the
// request is notified (back on the IO thread) as to whether the download should
// be allowed or denied.
//
// Invoking CanDownloadOnIOThread notifies the callback and may update the
// download status. The following details the various states:
// . Each NavigationController initially starts out allowing a download
//   (ALLOW_ONE_DOWNLOAD).
// . The first time CanDownloadOnIOThread is invoked the download is allowed and
//   the state changes to PROMPT_BEFORE_DOWNLOAD.
// . If the state is PROMPT_BEFORE_DOWNLOAD and the user clicks the mouse,
//   presses enter, the space bar or navigates to another page the state is
//   reset to ALLOW_ONE_DOWNLOAD.
// . If a download is attempted and the state is PROMPT_BEFORE_DOWNLOAD the user
//   is prompted as to whether the download is allowed or disallowed. The users
//   choice stays until the user navigates to a different host. For example, if
//   the user allowed the download, multiple downloads are allowed without any
//   user intervention until the user navigates to a different host.

class DownloadRequestManager :
    public base::RefCountedThreadSafe<DownloadRequestManager> {
 public:
  class TabDownloadState;

  // Download status for a particular page. See class description for details.
  enum DownloadStatus {
    ALLOW_ONE_DOWNLOAD,
    PROMPT_BEFORE_DOWNLOAD,
    ALLOW_ALL_DOWNLOADS,
    DOWNLOADS_NOT_ALLOWED
  };

  DownloadRequestManager(MessageLoop* io_loop, MessageLoop* ui_loop);
  ~DownloadRequestManager();

  // The callback from CanDownloadOnIOThread. This is invoked on the io thread.
  class Callback {
   public:
    virtual void ContinueDownload() = 0;
    virtual void CancelDownload() = 0;
  };

  // Returns the download status for a page. This does not change the state in
  // anyway.
  DownloadStatus GetDownloadStatus(TabContents* tab);

  // Updates the state of the page as necessary and notifies the callback.
  // WARNING: both this call and the callback are invoked on the io thread.
  //
  // DownloadRequestManager does not retain/release the Callback. It is up to
  // the caller to ensure the callback is valid until the request is complete.
  void CanDownloadOnIOThread(int render_process_host_id,
                             int render_view_id,
                             Callback* callback);

  // Invoked when the user presses the mouse, enter key or space bar. This may
  // change the download status for the page. See the class description for
  // details.
  void OnUserGesture(TabContents* tab);

 private:
  friend class DownloadRequestManagerTest;
  friend class TabDownloadState;

  // For unit tests. If non-null this is used instead of creating a dialog.
  class TestingDelegate {
   public:
    virtual bool ShouldAllowDownload() = 0;
  };
  static void SetTestingDelegate(TestingDelegate* delegate);

  // Gets the download state for the specified controller. If the
  // TabDownloadState does not exist and |create| is true, one is created.
  // See TabDownloadState's constructor description for details on the two
  // controllers.
  //
  // The returned TabDownloadState is owned by the DownloadRequestManager and
  // deleted when no longer needed (the Remove method is invoked).
  TabDownloadState* GetDownloadState(
      NavigationController* controller,
      NavigationController* originating_controller,
      bool create);

  // CanDownloadOnIOThread invokes this on the UI thread. This determines the
  // tab and invokes CanDownloadImpl.
  void CanDownload(int render_process_host_id,
                   int render_view_id,
                   Callback* callback);

  // Does the work of updating the download status on the UI thread and
  // potentially prompting the user.
  void CanDownloadImpl(TabContents* originating_tab,
                       Callback* callback);

  // Invoked on the UI thread. Schedules a call to NotifyCallback on the io
  // thread.
  void ScheduleNotification(Callback* callback, bool allow);

  // Notifies the callback. This *must* be invoked on the IO thread.
  void NotifyCallback(Callback* callback, bool allow);

  // Removes the specified TabDownloadState from the internal map and deletes
  // it. This has the effect of resetting the status for the tab to
  // ALLOW_ONE_DOWNLOAD.
  void Remove(TabDownloadState* state);

  // Two threads we use. NULL during testing, in which case messages are
  // dispatched immediately.
  MessageLoop* io_loop_;
  MessageLoop* ui_loop_;

  // Maps from tab to download state. The download state for a tab only exists
  // if the state is other than ALLOW_ONE_DOWNLOAD. Similarly once the state
  // transitions from anything but ALLOW_ONE_DOWNLOAD back to ALLOW_ONE_DOWNLOAD
  // the TabDownloadState is removed and deleted (by way of Remove).
  typedef std::map<NavigationController*, TabDownloadState*> StateMap;
  StateMap state_map_;

  static TestingDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DownloadRequestManager);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_MANAGER_H_
