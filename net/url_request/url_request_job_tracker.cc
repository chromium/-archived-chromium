// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "net/url_request/url_request_job_tracker.h"

#include "base/logging.h"
#include "net/url_request/url_request_job.h"

URLRequestJobTracker g_url_request_job_tracker;

URLRequestJobTracker::URLRequestJobTracker() {
}

URLRequestJobTracker::~URLRequestJobTracker() {
  DLOG_IF(WARNING, active_jobs_.size() != 0) <<
    "Leaking " << active_jobs_.size() << " URLRequestJob object(s), this could "
    "be because the URLRequest forgot to free it (bad), or if the program was "
    "terminated while a request was active (normal).";
}

void URLRequestJobTracker::AddNewJob(URLRequestJob* job) {
  active_jobs_.push_back(job);
  FOR_EACH_OBSERVER(JobObserver, observers_, OnJobAdded(job));
}

void URLRequestJobTracker::RemoveJob(URLRequestJob* job) {
  JobList::iterator iter = std::find(active_jobs_.begin(), active_jobs_.end(),
                                     job);
  if (iter == active_jobs_.end()) {
    NOTREACHED() << "Removing a non-active job";
    return;
  }
  active_jobs_.erase(iter);

  FOR_EACH_OBSERVER(JobObserver, observers_, OnJobRemoved(job));
}

void URLRequestJobTracker::OnJobDone(URLRequestJob* job,
                                     const URLRequestStatus& status) {
  FOR_EACH_OBSERVER(JobObserver, observers_, OnJobDone(job, status));
}

void URLRequestJobTracker::OnJobRedirect(URLRequestJob* job,
                                         const GURL& location,
                                         int status_code) {
  FOR_EACH_OBSERVER(JobObserver, observers_,
                    OnJobRedirect(job, location, status_code));
}

void URLRequestJobTracker::OnBytesRead(URLRequestJob* job,
                                       int byte_count) {
  FOR_EACH_OBSERVER(JobObserver, observers_,
                    OnBytesRead(job, byte_count));
}
