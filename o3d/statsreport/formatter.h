/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// Utility class to format metrics to a string suitable for posting to
// TB stats server.
#ifndef O3D_STATSREPORT_FORMATTER_H__
#define O3D_STATSREPORT_FORMATTER_H__

#include <strstream>
#include "base/basictypes.h"
#include "metrics.h"

namespace stats_report {

// A utility class that knows how to turn metrics into a string for
// reporting to the Toolbar stats server.
class Formatter {
 public:
  // @param name the name of the application to report stats against
  Formatter(const char *name, uint32 measurement_secs);
  ~Formatter();

  // Add metric to the output string
  void AddMetric(MetricBase *metric);

  // Add typed metrics to the output string
  // @{
  void AddCount(const char *name, uint64 value);
  void AddTiming(const char *name, uint64 num, uint64 avg, uint64 min,
                 uint64 max);
  void AddInteger(const char *name, uint64 value);
  void AddBoolean(const char *name, bool value);
  // @}

  // Terminates the output string and returns it.
  // It is an error to add metrics after output() is called.
  const char *output() {
    output_ << std::ends;
    return output_.str();
  }

 private:
  mutable std::strstream output_;

  DISALLOW_COPY_AND_ASSIGN(Formatter);
};

}  // namespace stats_report

#endif  // O3D_STATSREPORT_FORMATTER_H__
