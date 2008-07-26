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

#ifndef CHROME_BROWSER_NETWORK_STATUS_VIEW_H__
#define CHROME_BROWSER_NETWORK_STATUS_VIEW_H__

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/render_messages.h"
#include "chrome/browser/status_view.h"
#include "net/url_request/url_request_job_tracker.h"

class MessageLoop;
class RenderProcessHost;
class NavigationPerformanceViewer;
class PageLoadView;
namespace ChromeViews {
class HWNDViewContainer;
}

class NetworkStatusView : public StatusView {
 public:
  // button types
  enum {
    IDC_CONFIG_TRACKING_BUTTON = 101,
    IDC_CURRENT_STATUS_BUTTON,
    IDC_CONFIG_PROFILING_BUTTON,
    IDC_SHOW_PERFORMANCE_BUTTON,
    IDC_CLEAR,
    IDC_PAGE_LISTBOX,
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
    COMMAND_HANDLER_EX(IDC_CONFIG_PROFILING_BUTTON, BN_CLICKED, OnConfigProfilingClicked)
    COMMAND_HANDLER_EX(IDC_SHOW_PERFORMANCE_BUTTON, BN_CLICKED, OnShowPerformanceClicked)
    COMMAND_HANDLER_EX(IDC_PAGE_LISTBOX, LBN_DBLCLK, OnPageDoubleClicked)
    CHAIN_MSG_MAP(StatusView);
  END_MSG_MAP()

  bool is_tracking() const { return is_tracking_; }
 private:

  // Event handlers
  void OnConfigTrackingClicked(UINT code, int button_id, HWND hwnd);
  void OnCurrentStatusClicked(UINT code, int button_id, HWND hwnd);
  void OnClearClicked(UINT code, int button_id, HWND hwnd);
  void OnConfigProfilingClicked(UINT code, int button_id, HWND hwnd);
  void OnShowPerformanceClicked(UINT code, int button_id, HWND hwnd);
  void OnPageDoubleClicked(UINT code, int listbox_id, HWND hwnd);

  void AppendText(const std::wstring& text);

  // Hide/Show tracking output window
  void HideTrackingResults();
  void ShowTrackingResults();

  // Clear tracking output
  void ClearTrackingResults();

  // Hide/Show profiling output windows
  void HideProfilingResults();
  void ShowProfilingResults();

  // Clear profiling output
  void ClearProfilingResults();

  // Show detailed profiling results for the page with given index.
  void ReportPagePerformance(int page_index);

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

  // For performance profiling
  bool is_profiling_;
  int profiling_session_id_;
  scoped_ptr<NavigationPerformanceViewer> performance_viewer_;

  // Output of performance profiling

  // List of pages being profiled
  CListBox page_list_;

  // Textual report of selected page loading performance
  CEdit page_text_;

  // Graphical report of selected page loading performance
  PageLoadView* page_load_view_;
  // We don't use a scoped_ptr here since the page_view_container_ deletes
  // itself when it is destroyed automatically by its parent window.
  ChromeViews::HWNDViewContainer* page_view_container_;

  // Current page being selected
  int current_page_index_;

  DISALLOW_EVIL_CONSTRUCTORS(NetworkStatusView);
};

#endif  // #ifndef CHROME_BROWSER_NETWORK_STATUS_VIEW_H__
