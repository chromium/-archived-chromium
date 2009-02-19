// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/sdch_dictionary_fetcher.h"

#include "chrome/browser/profile.h"
#include "net/url_request/url_request_status.h"

void SdchDictionaryFetcher::Schedule(const GURL& dictionary_url) {
  // Avoid pushing duplicate copy onto queue.  We may fetch this url again later
  // and get a different dictionary, but there is no reason to have it in the
  // queue twice at one time.
  if (!fetch_queue_.empty() && fetch_queue_.back() == dictionary_url) {
    SdchManager::SdchErrorRecovery(
        SdchManager::DICTIONARY_ALREADY_SCHEDULED_TO_DOWNLOAD);
    return;
  }
  fetch_queue_.push(dictionary_url);
  ScheduleDelayedRun();
}

void SdchDictionaryFetcher::ScheduleDelayedRun() {
  if (fetch_queue_.empty() || current_fetch_.get() || task_is_pending_)
    return;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&SdchDictionaryFetcher::StartFetching),
      kMsDelayFromRequestTillDownload);
  task_is_pending_ = true;
}

void SdchDictionaryFetcher::StartFetching() {
  DCHECK(task_is_pending_);
  task_is_pending_ = false;

  URLRequestContext* context = Profile::GetDefaultRequestContext();
  if (!context) {
    // Shutdown in progress.
    // Simulate handling of all dictionary requests by clearing queue.
    while (!fetch_queue_.empty())
      fetch_queue_.pop();
    return;
  }

  current_fetch_.reset(new URLFetcher(fetch_queue_.front(), URLFetcher::GET,
                                      this));
  fetch_queue_.pop();
  current_fetch_->set_request_context(context);
  current_fetch_->Start();
}

void SdchDictionaryFetcher::OnURLFetchComplete(const URLFetcher* source,
                                               const GURL& url,
                                               const URLRequestStatus& status,
                                               int response_code,
                                               const ResponseCookies& cookies,
                                               const std::string& data) {
  if ((200 == response_code) && (status.status() == URLRequestStatus::SUCCESS))
    SdchManager::Global()->AddSdchDictionary(data, url);
  current_fetch_.reset(NULL);
  ScheduleDelayedRun();
}
