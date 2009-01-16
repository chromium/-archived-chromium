// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

