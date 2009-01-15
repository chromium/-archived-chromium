// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/network_status_view.h"

#include <stdio.h>

#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/views/root_view.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace {
const wchar_t kTitleMsg[] = L"Network Status";
const wchar_t kStartTrackingMsg[] = L"Start I/O Tracking";
const wchar_t kStopTrackingMsg[] = L"Stop I/O Tracking";

const wchar_t kShowIOStatusMsg[] = L"Show Current I/O Status";
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

  tracker_->DetachView();
}

const std::wstring NetworkStatusView::GetDefaultTitle() {
  return kTitleMsg;
}

void NetworkStatusView::OnCreate(const CRect& rect) {
  CreateButton(IDC_CONFIG_TRACKING_BUTTON, kStartTrackingMsg);
  CreateButton(IDC_CURRENT_STATUS_BUTTON, kShowIOStatusMsg);
  CreateButton(IDC_CLEAR, kClearOutputMsg);

  is_tracking_ = false;

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
}

void NetworkStatusView::AppendText(const std::wstring& text) {
  text_area_.AppendText(text.c_str());
}

void NetworkStatusView::HideTrackingResults() {
  text_area_.ShowWindow(SW_HIDE);
}

void NetworkStatusView::ShowTrackingResults() {
  text_area_.ShowWindow(SW_SHOW);
}

void NetworkStatusView::ClearTrackingResults() {
  text_area_.SetSelAll();
  text_area_.Clear();
}

//-----------------------------------------------------------------------------

// main thread:
NetworkStatusView::JobTracker::JobTracker(NetworkStatusView* view)
    : view_(view),
      view_message_loop_(MessageLoop::current()) {
}

// main thread:
void NetworkStatusView::JobTracker::InvokeOnIOThread(void (JobTracker::*m)()) {
  base::Thread* thread = g_browser_process->io_thread();
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

