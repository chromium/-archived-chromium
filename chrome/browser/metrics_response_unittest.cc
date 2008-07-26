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

#include <string>

#include "chrome/browser/metrics_response.h"
#include "chrome/browser/metrics_service.h"
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
  for (int i = 0; i < arraysize(response_cases); ++i) {
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
  for (int i = 0; i < arraysize(bogus_responses); ++i) {
    MetricsResponse response(bogus_responses[i]);
    EXPECT_FALSE(response.valid());
  }
}

