// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Records IO statistics associated with a URLRequestJob.
// See description in navigation_profiler.h for an overview of perf profiling.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_METRICS_H_
#define NET_URL_REQUEST_URL_REQUEST_JOB_METRICS_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

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
  base::TimeTicks start_time_;

  // Time when the job is done.
  base::TimeTicks end_time_;

  // Total number of bytes the job reads from underline IO.
  int64 total_bytes_read_;

  // Number of IO read operations the job issues.
  int number_of_read_IO_;

  // Final status of the job.
  bool success_;

  // Append the text report of the frame loading to the input string.
  void AppendText(std::wstring* text);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_METRICS_H_
