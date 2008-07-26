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

#include <algorithm>

#include "net/url_request/url_request_job_tracker.h"

#include "base/logging.h"
#include "net/url_request/url_request_job.h"

URLRequestJobTracker g_url_request_job_tracker;

URLRequestJobTracker::URLRequestJobTracker() {
}

URLRequestJobTracker::~URLRequestJobTracker() {
  DLOG_IF(WARNING, active_jobs_.size() != 0) <<
    "Leaking " << active_jobs_.size() << " URLRequestJob object(s), this could be "
    "because the URLRequest forgot to free it (bad), or if the program was "
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
