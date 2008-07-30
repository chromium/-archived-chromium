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

#include <stdio.h>

#include "chrome/browser/network_status_view.h"

#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/navigation_profiler.h"
#include "chrome/browser/navigation_performance_viewer.h"
#include "chrome/browser/page_load_tracker.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/views/hwnd_view_container.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace {
const wchar_t kTitleMsg[] = L"Network Status";
const wchar_t kStartTrackingMsg[] = L"Start I/O Tracking";
const wchar_t kStopTrackingMsg[] = L"Stop I/O Tracking";

const wchar_t kStartProfilingMsg[] = L"Start Profiling";
const wchar_t kStopProfilingMsg[] = L"Stop Profiling";

const wchar_t kShowIOStatusMsg[] = L"Show Current I/O Status";
const wchar_t kShowPerformanceMsg[] = L"Show Performance";
const wchar_t kRefreshPerformanceMsg[] = L"Refresh Performance";
const wchar_t kClearOutputMsg[] = L"Clear Output";

// Returns a string representing the URL, handling the case where the spec
// is invalid.
std::wstring StringForURL(const GURL& url) {
  if (url.is_valid())
    return UTF8ToWide(url.spec());
  return UTF8ToWide(url.possibly_invalid_spec()) + L" (invalid)";
}

std::wstring URLForJob(URLRequestJob* job) {
  URLRequest* request = job->request();
  if (request)
    return StringForURL(request->url());
  return std::wstring(L"(orphaned)");
}

}  // namespace

NetworkStatusView::NetworkStatusView()
    : StatusView(TAB_CONTENTS_NETWORK_STATUS_VIEW) {
  tracker_ = new JobTracker(this);
}

NetworkStatusView::~NetworkStatusView() {
  if (monospaced_font_)
    DeleteObject(monospaced_font_);

  if (is_tracking_) {
    tracker_->StopTracking();
    is_tracking_ = false;
  }

  if (is_profiling_) {
    g_navigation_profiler.StopProfiling(profiling_session_id_);
    is_profiling_ = false;
  }

  tracker_->DetachView();
}

const std::wstring NetworkStatusView::GetDefaultTitle() {
  return kTitleMsg;
}

void NetworkStatusView::OnCreate(const CRect& rect) {
  CreateButton(IDC_CONFIG_TRACKING_BUTTON, kStartTrackingMsg);
  CreateButton(IDC_CURRENT_STATUS_BUTTON, kShowIOStatusMsg);
  CreateButton(IDC_CONFIG_PROFILING_BUTTON, kStartProfilingMsg);
  CreateButton(IDC_SHOW_PERFORMANCE_BUTTON, kShowPerformanceMsg);
  CreateButton(IDC_CLEAR, kClearOutputMsg);

  is_tracking_ = false;
  is_profiling_ = false;

  // Initialize the text box for network tracking
  // Don't worry about the size, we'll resize when we get WM_SIZE
  text_area_.Create(m_hWnd, const_cast<CRect&>(rect), NULL,
                    WS_CHILD | WS_HSCROLL | WS_VSCROLL |
                    ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL, 0);

  // This raises the maximum number of chars from 32K to some large maximum,
  // probably 2GB. 32K is not nearly enough for our use-case.
  text_area_.SendMessageW(EM_SETLIMITTEXT, 0, 0);

  // make a monospaced font for the edit control
  LOGFONT lf = {0};
  lf.lfHeight = 16;
  wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Courier New");
  monospaced_font_ = CreateFontIndirect(&lf);
  text_area_.SetFont(monospaced_font_);

  HideTrackingResults();

  // Initialize the view for performance profiling
  // Don't worry about the size, we'll resize when we get WM_SIZE

  // List of loaded pages
  page_list_.Create(m_hWnd, const_cast<CRect&>(rect), NULL,
                    WS_CHILD | WS_HSCROLL | WS_VSCROLL | LBS_STANDARD, 0,
                    IDC_PAGE_LISTBOX);

  // Textual report of page loading
  page_text_.Create(m_hWnd, const_cast<CRect&>(rect), NULL,
                    WS_CHILD | WS_HSCROLL | WS_VSCROLL |
                    ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL, 0);
  page_text_.SetFont(monospaced_font_);
  // Raise the maximum number of chars from 32K to some large maximum.
  page_text_.SendMessageW(EM_SETLIMITTEXT, 0, 0);

  // Graphical report of page loading
  page_load_view_ = new PageLoadView();
  page_view_container_ = new ChromeViews::HWNDViewContainer;
  page_view_container_->Init(m_hWnd, gfx::Rect(rect), false);
  page_view_container_->SetContentsView(page_load_view_);

  HideProfilingResults();
}

void NetworkStatusView::OnSize(const CRect& rect) {
  // re-layout the edit control
  text_area_.MoveWindow(rect);

  // re-layout the performance view
  CRect new_rect(rect);
  int list_width = rect.Width();
  int list_height = static_cast<int>(rect.Height() / 5);
  int page_width = rect.Width() / 2;
  int page_height = static_cast<int>(rect.Height() * 4 / 5);

  new_rect.SetRect(rect.left, rect.top,
                   rect.left + list_width, rect.top + list_height);
  page_list_.MoveWindow(new_rect);

  new_rect.SetRect(rect.left,
                   rect.top + list_height,
                   rect.left + page_width,
                   rect.top + list_height + page_height);
  page_text_.MoveWindow(new_rect);

  page_view_container_->MoveWindow(rect.left + page_width + kLayoutPadding,
                                   rect.top + list_height,
                                   page_width - kLayoutPadding, page_height,
                                   TRUE);
}

void NetworkStatusView::OnConfigTrackingClicked(UINT code, int button_id,
                                                HWND hwnd) {
  if (is_tracking_) {
    tracker_->StopTracking();
    is_tracking_ = false;

    SetButtonText(IDC_CONFIG_TRACKING_BUTTON, kStartTrackingMsg);
  } else {
    tracker_->StartTracking();
    is_tracking_ = true;

    ClearTrackingResults();
    ShowTrackingResults();

    SetButtonText(IDC_CONFIG_TRACKING_BUTTON, kStopTrackingMsg);
  }
}

void NetworkStatusView::OnCurrentStatusClicked(UINT code, int button_id,
                                               HWND hwnd) {
  ShowTrackingResults();
  if (is_tracking_) {
    tracker_->ReportStatus();
  }
}

void NetworkStatusView::OnClearClicked(UINT code, int button_id, HWND hwnd) {
  ClearTrackingResults();
  ClearProfilingResults();
}

void NetworkStatusView::OnConfigProfilingClicked(UINT code, int button_id,
                                                 HWND hwnd) {
  if (is_profiling_) {
    g_navigation_profiler.StopProfiling(profiling_session_id_);
    is_profiling_ = false;

    HideProfilingResults();

    SetButtonText(IDC_CONFIG_PROFILING_BUTTON, kStartProfilingMsg);
  } else {
    profiling_session_id_ = g_navigation_profiler.StartProfiling();
    is_profiling_ = true;

    HideTrackingResults();
    ClearProfilingResults();

    performance_viewer_.reset(
        new NavigationPerformanceViewer(profiling_session_id_));

    ShowProfilingResults();

    SetButtonText(IDC_CONFIG_PROFILING_BUTTON, kStopProfilingMsg);
    SetButtonText(IDC_SHOW_PERFORMANCE_BUTTON, kShowPerformanceMsg);
  }
}

void NetworkStatusView::OnShowPerformanceClicked(UINT code, int button_id,
                                                 HWND hwnd) {
  HideTrackingResults();
  ShowProfilingResults();

  if (!is_profiling_)
    return;

  int num_pages =
      g_navigation_profiler.RetrieveVisitedPages(performance_viewer_.get());

  // Refresh display if there are new page profiling results
  if (num_pages > 0) {
    // Display the list of page URLs
    page_list_.ResetContent();
    int size = performance_viewer_->GetSize();
    for (int i = 0; i < size; ++i) {
      PageLoadTracker* page = performance_viewer_->GetPageReference(i);
      page_list_.InsertString(i, StringForURL(page->url()).c_str());
    }

    page_list_.SetCurSel(current_page_index_);
    ReportPagePerformance(current_page_index_);

    SetButtonText(IDC_SHOW_PERFORMANCE_BUTTON, kRefreshPerformanceMsg);
  }
}

void NetworkStatusView::OnPageDoubleClicked(UINT code, int command_id,
                                            HWND window) {
  int index = page_list_.GetCurSel();
  if (index == LB_ERR)
    return;

  current_page_index_ = index;
  ReportPagePerformance(current_page_index_);
}

void NetworkStatusView::AppendText(const std::wstring& text) {
  text_area_.AppendText(text.c_str());
}

void NetworkStatusView::HideTrackingResults() {
  text_area_.ShowWindow(SW_HIDE);
}

void NetworkStatusView::ShowTrackingResults() {
  HideProfilingResults();
  text_area_.ShowWindow(SW_SHOW);
}

void NetworkStatusView::ClearTrackingResults() {
  text_area_.SetSelAll();
  text_area_.Clear();
}

void NetworkStatusView::HideProfilingResults() {
  page_list_.ShowWindow(SW_HIDE);
  page_text_.ShowWindow(SW_HIDE);
  page_view_container_->ShowWindow(SW_HIDE);
}

void NetworkStatusView::ShowProfilingResults() {
  HideTrackingResults();
  page_list_.ShowWindow(SW_SHOW);
  page_text_.ShowWindow(SW_SHOW);
  page_view_container_->ShowWindow(SW_SHOW);
}

void NetworkStatusView::ClearProfilingResults() {
  page_list_.ResetContent();
  current_page_index_ = 0;

  page_text_.SetSelAll();
  page_text_.Clear();

  page_load_view_->SetPage(NULL);
  page_view_container_->GetRootView()->SchedulePaint();

  if (performance_viewer_.get())
    performance_viewer_->Reset();
}

void NetworkStatusView::ReportPagePerformance(int page_index) {
    PageLoadTracker* page = performance_viewer_->GetPageReference(page_index);

    page_text_.SetSelAll();
    page_text_.Clear();

    if (page != NULL) {
      std::wstring text;
      page->AppendText(&text);
      page_text_.AppendText(text.c_str());
    }

    page_load_view_->SetPage(page);
    page_view_container_->GetRootView()->SchedulePaint();
}

//-----------------------------------------------------------------------------

// main thread:
NetworkStatusView::JobTracker::JobTracker(NetworkStatusView* view)
    : view_(view),
      view_message_loop_(MessageLoop::current()) {
}

// main thread:
void NetworkStatusView::JobTracker::InvokeOnIOThread(void (JobTracker::*m)()) {
  Thread* thread = g_browser_process->io_thread();
  if (!thread)
    return;
  thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this, m));
}

// main thread:
void NetworkStatusView::JobTracker::StartTracking() {
  DCHECK(MessageLoop::current() == view_message_loop_);
  DCHECK(view_);
  InvokeOnIOThread(&JobTracker::OnStartTracking);
}

// main thread:
void NetworkStatusView::JobTracker::StopTracking() {
  DCHECK(MessageLoop::current() == view_message_loop_);
  // The tracker should not be deleted before it is removed from observer
  // list.
  AddRef();
  InvokeOnIOThread(&JobTracker::OnStopTracking);
}

// main thread:
void NetworkStatusView::JobTracker::ReportStatus() {
  DCHECK(MessageLoop::current() == view_message_loop_);
  InvokeOnIOThread(&JobTracker::OnReportStatus);
}

// main thread:
void NetworkStatusView::JobTracker::OnAppendText(const std::wstring& text) {
  DCHECK(MessageLoop::current() == view_message_loop_);
  if (view_ && view_->is_tracking_)
    view_->AppendText(text);
}

// IO thread:
void NetworkStatusView::JobTracker::AppendText(const std::wstring& text) {
  DCHECK(MessageLoop::current() != view_message_loop_);
  view_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &JobTracker::OnAppendText, text));
}

// IO thread:
void NetworkStatusView::JobTracker::OnStartTracking() {
  DCHECK(MessageLoop::current() != view_message_loop_);
  g_url_request_job_tracker.AddObserver(this);
}

// IO thread:
void NetworkStatusView::JobTracker::OnStopTracking() {
  DCHECK(MessageLoop::current() != view_message_loop_);
  g_url_request_job_tracker.RemoveObserver(this);
  // Balance the AddRef() in StopTracking() called in main thread.
  Release();
}

// IO thread:
void NetworkStatusView::JobTracker::OnReportStatus() {
  DCHECK(MessageLoop::current() != view_message_loop_);

  std::wstring text(L"\r\n===== Active Job Summary =====\r\n");

  URLRequestJobTracker::JobIterator begin_job =
      g_url_request_job_tracker.begin();
  URLRequestJobTracker::JobIterator end_job = g_url_request_job_tracker.end();
  int orphaned_count = 0;
  int regular_count = 0;
  for (URLRequestJobTracker::JobIterator cur = begin_job;
       cur != end_job; ++cur) {
    URLRequestJob* job = (*cur);
    URLRequest* request = job->request();
    if (!request) {
      orphaned_count++;
      continue;
    }

    regular_count++;

    // active state
    if (job->is_done())
      text.append(L"  Done:   ");
    else
      text.append(L"  Active: ");

    // URL
    text.append(StringForURL(request->url()));
    text.append(L"\r\n");
  }

  if (regular_count == 0)
    text.append(L"  (No active jobs)\r\n");

  if (orphaned_count) {
    wchar_t buf[64];
    swprintf(buf, arraysize(buf), L"  %d orphaned jobs\r\n", orphaned_count);
    text.append(buf);
  }

  text.append(L"=====\r\n\r\n");
  AppendText(text);
}

// IO thread:
void NetworkStatusView::JobTracker::OnJobAdded(URLRequestJob* job) {
  DCHECK(MessageLoop::current() != view_message_loop_);

  std::wstring text(L"+ New job : ");
  text.append(URLForJob(job));
  text.append(L"\r\n");
  AppendText(text);
}

// IO thread:
void NetworkStatusView::JobTracker::OnJobRemoved(URLRequestJob* job) {
  DCHECK(MessageLoop::current() != view_message_loop_);
}

// IO thread:
void NetworkStatusView::JobTracker::OnJobDone(URLRequestJob* job,
                                              const URLRequestStatus& status) {
  DCHECK(MessageLoop::current() != view_message_loop_);

  std::wstring text;
  if (status.is_success()) {
    text.assign(L"- Complete: ");
  } else if (status.status() == URLRequestStatus::CANCELED) {
    text.assign(L"- Canceled: ");
  } else if (status.status() == URLRequestStatus::HANDLED_EXTERNALLY) {
    text.assign(L"- Handled externally: ");
  } else {
    wchar_t buf[32];
    swprintf(buf, arraysize(buf), L"Failed with %d: ", status.os_error());
    text.assign(buf);
  }

  text.append(URLForJob(job));
  text.append(L"\r\n");
  AppendText(text);
}

// IO thread:
void NetworkStatusView::JobTracker::OnJobRedirect(URLRequestJob* job,
                                                  const GURL& location,
                                                  int status_code) {
  DCHECK(MessageLoop::current() != view_message_loop_);

  std::wstring text(L"- Redirect: ");
  text.append(URLForJob(job));
  text.append(L"\r\n  ");

  wchar_t buf[16];
  swprintf(buf, arraysize(buf), L"(%d) to: ", status_code);
  text.append(buf);

  text.append(StringForURL(location));
  text.append(L"\r\n");
  AppendText(text);
}

void NetworkStatusView::JobTracker::OnBytesRead(URLRequestJob* job,
                                                int byte_count) {
}
