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

#ifndef BASE_PERFTTIMER_H__
#define BASE_PERFTTIMER_H__

#include <string>
#include "base/basictypes.h"
#include "base/time.h"

// ----------------------------------------------------------------------
// Initializes and finalizes the perf log. These functions should be
// called at the beginning and end (respectively) of running all the
// performance tests. The init function returns true on success.
// ----------------------------------------------------------------------
bool InitPerfLog(const char* log_file);
void FinalizePerfLog();

// ----------------------------------------------------------------------
// LogPerfResult
//   Writes to the perf result log the given 'value' resulting from the
//   named 'test'. The units are to aid in reading the log by people.
// ----------------------------------------------------------------------
void LogPerfResult(const char* test_name, double value, const char* units);

// ----------------------------------------------------------------------
// PerfTimer
//   A simple wrapper around Now()
// ----------------------------------------------------------------------
class PerfTimer {
 public:
  PerfTimer() {
    begin_ = TimeTicks::Now();
  }

  // Returns the time elapsed since object construction
  TimeDelta Elapsed() const {
    return TimeTicks::Now() - begin_;
  }

 private:
  TimeTicks begin_;
};

// ----------------------------------------------------------------------
// PerfTimeLogger
//   Automates calling LogPerfResult for the common case where you want
//   to measure the time that something took. Call Done() when the test
//   is complete if you do extra work after the test or there are stack
//   objects with potentially expensive constructors. Otherwise, this
//   class with automatically log on destruction.
// ----------------------------------------------------------------------
class PerfTimeLogger {
 public:
  explicit PerfTimeLogger(const char* test_name)
      : logged_(false),
        test_name_(test_name) {
  }

  ~PerfTimeLogger() {
    if (!logged_)
      Done();
  }

  void Done() {
    // we use a floating-point millisecond value because it is more
    // intuitive than microseconds and we want more precision than
    // integer milliseconds
    LogPerfResult(test_name_.c_str(), timer_.Elapsed().InMillisecondsF(), "ms");
    logged_ = true;
  }

 private:
  bool logged_;
  std::string test_name_;
  PerfTimer timer_;
};

#endif  // BASE_PERFTTIMER_H__
