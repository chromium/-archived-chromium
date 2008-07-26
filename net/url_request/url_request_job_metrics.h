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

// Records IO statistics associated with a URLRequestJob.
// See description in navigation_profiler.h for an overview of perf profiling.

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

#ifndef BASE_URL_REQUEST_URL_REQUEST_JOB_METRICS_H__
#define BASE_URL_REQUEST_URL_REQUEST_JOB_METRICS_H__

class URLRequestJobMetrics {
 public:
  URLRequestJobMetrics() : total_bytes_read_(0), number_of_read_IO_(0) { }
  ~URLRequestJobMetrics() { }

  // The original url the job has been created for.
  scoped_ptr<GURL> original_url_;

  // The actual url the job connects to. If the actual url is same as the
  // original url, url_ is empty.
  scoped_ptr<GURL> url_;

  // Time when the job starts.
  TimeTicks start_time_;

  // Time when the job is done.
  TimeTicks end_time_;

  // Total number of bytes the job reads from underline IO.
  int total_bytes_read_;

  // Number of IO read operations the job issues.
  int number_of_read_IO_;

  // Final status of the job.
  bool success_;

  // Append the text report of the frame loading to the input string.
  void AppendText(std::wstring* text);
};

#endif  // BASE_URL_REQUEST_URL_REQUEST_JOB_METRICS_H__
