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

// This class exists to interpret the response from the metrics server.

#ifndef CHROME_BROWSER_METRICS_RESPONSE_H__
#define CHROME_BROWSER_METRICS_RESPONSE_H__

#include <string>

#include "base/basictypes.h"

class MetricsResponse {
public:
  // Parses metrics response XML into the information we care about
  // (how often to send metrics info, which info to send).
  MetricsResponse(const std::string& response_xml);

  // True if the XML passed to the constructor was valid and parseable.
  bool valid() { return valid_; }

  // Each flag (except NONE) defined here represents one type of metrics
  // event that the server is interested in.
  enum CollectorType {
    COLLECTOR_NONE = 0x0,
    COLLECTOR_PROFILE = 0x1,
    COLLECTOR_WINDOW = 0x2,
    COLLECTOR_DOCUMENT = 0x4,
    COLLECTOR_UI = 0x8
  };

  // This is the collection of CollectorTypes that are desired by the
  // server, ORed together into one value.
  int collectors() { return collectors_; }

  // Returns true if the given CollectorType is desired by the server.
  bool collector_active(CollectorType type) { return !!(collectors_ & type); }

  // Returns the maximum number of event that the server wants in each
  // metrics log sent.  (If 0, no value was provided.)
  int events() { return events_; }

  // Returns the size of the time interval that the server wants us to include
  // in each log (in seconds).  (If 0, no value was provided.)
  int interval() { return interval_; }

private:
  bool valid_;
  int collectors_;
  int events_;
  int interval_;

  DISALLOW_EVIL_CONSTRUCTORS(MetricsResponse);
};

#endif  // CHROME_BROWSER_METRICS_RESPONSE_H__
