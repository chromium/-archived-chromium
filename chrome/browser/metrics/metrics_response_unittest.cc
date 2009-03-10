// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/metrics/metrics_response.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class MetricsResponseTest : public testing::Test {
  };
};

static const char kNoLogResponse[] =
    "<response xmlns=\"http://www.mozilla.org/metrics\"><config>"
    "</config></response>";

static const char kLogResponse1[] =
    "<response xmlns=\"http://www.mozilla.org/metrics\"><config>"
      "<collectors>"
        "<collector type=\"profile\"/>"
        "<collector type=\"document\"/>"
        "<collector type=\"window\"/>"
        "<collector type=\"ui\"/>"
      "</collectors>"
      "<limit events=\"500\"/><upload interval=\"600\"/>"
    "</config></response>";

static const char kLogResponse2[] =
    "<response xmlns=\"http://www.mozilla.org/metrics\"><config>"
      "<collectors>"
        "<collector type=\"profile\"/>"
        "<collector type=\"document\"/>"
        "<collector type=\"window\"/>"
      "</collectors>"
      "<limit events=\"250\"/><upload interval=\"900\"/>"
    "</config></response>";


static const struct ResponseCase {
  const char* response_xml;
  int collectors;
  int events;
  int interval;
  bool profile_active;
  bool window_active;
  bool document_active;
  bool ui_active;
} response_cases[] = {
  {
    kNoLogResponse,
    MetricsResponse::COLLECTOR_NONE,
    0, 0,
    false, false, false, false
  },
  {
    kLogResponse1,
    MetricsResponse::COLLECTOR_PROFILE |
        MetricsResponse::COLLECTOR_DOCUMENT |
        MetricsResponse::COLLECTOR_WINDOW |
        MetricsResponse::COLLECTOR_UI,
    500, 600,
    true, true, true, true
  },
  {
    kLogResponse2,
    MetricsResponse::COLLECTOR_PROFILE |
        MetricsResponse::COLLECTOR_DOCUMENT |
        MetricsResponse::COLLECTOR_WINDOW,
    250, 900,
    true, true, true, false
  },
};

TEST(MetricsResponseTest, ParseResponse) {
  for (size_t i = 0; i < arraysize(response_cases); ++i) {
    ResponseCase rcase = response_cases[i];
    MetricsResponse response(rcase.response_xml);
    EXPECT_TRUE(response.valid());
    EXPECT_EQ(rcase.collectors, response.collectors()) <<
        "Mismatch in case " << i;
    EXPECT_EQ(rcase.events, response.events()) << "Mismatch in case " << i;
    EXPECT_EQ(rcase.interval, response.interval()) << "Mismatch in case " << i;
    EXPECT_EQ(rcase.profile_active,
              response.collector_active(MetricsResponse::COLLECTOR_PROFILE)) <<
              "Mismatch in case " << i;
    EXPECT_EQ(rcase.window_active,
              response.collector_active(MetricsResponse::COLLECTOR_WINDOW)) <<
              "Mismatch in case " << i;
    EXPECT_EQ(rcase.document_active,
              response.collector_active(MetricsResponse::COLLECTOR_DOCUMENT)) <<
              "Mismatch in case " << i;
    EXPECT_EQ(rcase.ui_active,
              response.collector_active(MetricsResponse::COLLECTOR_UI)) <<
              "Mismatch in case " << i;
  }
}

static const char* bogus_responses[] = {"", "<respo"};
TEST(MetricsResponseTest, ParseBogusResponse) {
  for (size_t i = 0; i < arraysize(bogus_responses); ++i) {
    MetricsResponse response(bogus_responses[i]);
    EXPECT_FALSE(response.valid());
  }
}
