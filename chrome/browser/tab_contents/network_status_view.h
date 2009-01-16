// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_NETWORK_STATUS_VIEW_H_
#define CHROME_BROWSER_TAB_CONTENTS_NETWORK_STATUS_VIEW_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/render_messages.h"
#include "chrome/browser/tab_contents/status_view.h"
#include "net/url_request/url_request_job_tracker.h"

class MessageLoop;
class RenderProcessHost;
class NavigationPerformanceViewer;
class PageLoadView;

class NetworkStatusView : public StatusView {
 public:
  // button types
  enum {
    IDC_CONFIG_TRACKING_BUTTON = 101,
    IDC_CURRENT_STATUS_BUTTON,
    IDC_CLEAR,
  };

  NetworkStatusView();
  virtual ~NetworkStatusView();

  // TabContents overrides
  virtual const std::wstring GetDefaultTitle();

  // StatusView implementation
  virtual void OnCreate(const CRect& rect);
  virtual void OnSize(const CRect& rect);

  BEGIN_MSG_MAP(NetworkStatusView)
    COMMAND_HANDLER_EX(IDC_CONFIG_TRACKING_BUTTON, BN_CLICKED, OnConfigTrackingClicked)
    COMMAND_HANDLER_EX(IDC_CURRENT_STATUS_BUTTON, BN_CLICKED, OnCurrentStatusClicked)
    COMMAND_HANDLER_EX(IDC_CLEAR, BN_CLICKED, OnClearClicked)
    CHAIN_MSG_MAP(StatusView);
  END_MSG_MAP()

  bool is_tracking() const { return is_tracking_; }

 private:

  // Event handlers
  void OnConfigTrackingClicked(UINT code, int button_id, HWND hwnd);
  void OnCurrentStatusClicked(UINT code, int button_id, HWND hwnd);
  void OnClearClicked(UINT code, int button_id, HWND hwnd);

  void AppendText(const std::wstring& text);

  // Hide/Show tracking output window
  void HideTrackingResults();
  void ShowTrackingResults();

  // Clear tracking output
  void ClearTrackingResults();

  // A JobTracker is allocated to monitor network jobs running on the IO
  // thread.  This allows the NetworkStatusView to remain single-threaded.
  class JobTracker : public URLRequestJobTracker::JobObserver,
                     public base::RefCountedThreadSafe<JobTracker> {
   public:
    JobTracker(NetworkStatusView* view);

    // Called by the NetworkStatusView on the main application thread.
    void StartTracking();
    void StopTracking();
    void ReportStatus();

    // URLRequestJobTracker::JobObserver methods (called on the IO thread):
    virtual void OnJobAdded(URLRequestJob* job);
    virtual void OnJobRemoved(URLRequestJob* job);
    virtual void OnJobDone(URLRequestJob* job, const URLRequestStatus& status);
    virtual void OnJobRedirect(URLRequestJob* job, const GURL& location,
                               int status_code);
    virtual void OnBytesRead(URLRequestJob* job, int byte_count);

    // The JobTracker may be deleted after NetworkStatusView is deleted.
    void DetachView() { view_ = NULL; }

   private:
    void InvokeOnIOThread(void (JobTracker::*method)());

    // Called on the IO thread
    void OnStartTracking();
    void OnStopTracking();
    void OnReportStatus();
    void AppendText(const std::wstring& text);

    // Called on the main thread
    void OnAppendText(const std::wstring& text);

    NetworkStatusView* view_;
    MessageLoop* view_message_loop_;
  };
  friend class JobTracker;

  scoped_refptr<JobTracker> tracker_;

  bool is_tracking_;

  // Textual output of network tracking
  CEdit text_area_;

  HFONT monospaced_font_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStatusView);
};

#endif  // #ifndef CHROME_BROWSER_TAB_CONTENTS_NETWORK_STATUS_VIEW_H_

