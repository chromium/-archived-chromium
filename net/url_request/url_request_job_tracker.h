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

#ifndef BASE_URL_REQUEST_URL_REQUEST_JOB_TRACKER_H__
#define BASE_URL_REQUEST_URL_REQUEST_JOB_TRACKER_H__

#include <vector>

#include "base/observer_list.h"
#include "net/url_request/url_request_status.h"

class URLRequestJob;
class GURL;

// This class maintains a list of active URLRequestJobs for debugging purposes.
// This allows us to warn on leaked jobs and also allows an observer to track
// what is happening, for example, for the network status monitor.
//
// NOTE: URLRequest is single-threaded, so this class should only be used on
// the same thread where all of the application's URLRequest calls are made.
//
class URLRequestJobTracker {
 public:
  typedef std::vector<URLRequestJob*> JobList;
  typedef JobList::const_iterator JobIterator;

  // The observer's methods are called on the thread that called AddObserver.
  class JobObserver {
   public:
    // Called after the given job has been added to the list
    virtual void OnJobAdded(URLRequestJob* job) = 0;

    // Called after the given job has been removed from the list
    virtual void OnJobRemoved(URLRequestJob* job) = 0;

    // Called when the given job has completed, before notifying the request
    virtual void OnJobDone(URLRequestJob* job,
                           const URLRequestStatus& status) = 0;

    // Called when the given job is about to follow a redirect to the given
    // new URL. The redirect type is given in status_code
    virtual void OnJobRedirect(URLRequestJob* job, const GURL& location,
                               int status_code) = 0;

    // Called when a new chunk of bytes has been read for the given job. The
    // byte count is the number of bytes for that read event only.
    virtual void OnBytesRead(URLRequestJob* job, int byte_count) = 0;
  };

  URLRequestJobTracker();
  ~URLRequestJobTracker();

  // adds or removes an observer from the list.  note, these methods should
  // only be called on the same thread where URLRequest objects are used.
  void AddObserver(JobObserver* observer) {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(JobObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // adds or removes the job from the active list, should be called by the
  // job constructor and destructor. Note: don't use "AddJob" since that
  // is #defined by windows.h :(
  void AddNewJob(URLRequestJob* job);
  void RemoveJob(URLRequestJob* job);

  // Job status change notifications
  void OnJobDone(URLRequestJob* job, const URLRequestStatus& status);
  void OnJobRedirect(URLRequestJob* job, const GURL& location,
                     int status_code);

  // Bytes read notifications.
  void OnBytesRead(URLRequestJob* job, int byte_count);

  // allows iteration over all active jobs
  JobIterator begin() const {
    return active_jobs_.begin();
  }
  JobIterator end() const {
    return active_jobs_.end();
  }

 private:
  ObserverList<JobObserver> observers_;
  JobList active_jobs_;
};

extern URLRequestJobTracker g_url_request_job_tracker;

#endif  // BASE_URL_REQUEST_URL_REQUEST_JOB_TRACKER_H__
