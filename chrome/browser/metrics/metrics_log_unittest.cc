// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/metrics/metrics_log.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace {
  class MetricsLogTest : public testing::Test {
  };
};

TEST(MetricsLogTest, EmptyRecord) {
  std::string expected_output = "<log clientid=\"bogus client ID\"/>";

  MetricsLog log("bogus client ID", 0);
  log.CloseLog();

  int size = log.GetEncodedLogSize();
  ASSERT_GT(size, 0);

  std::string encoded;
  ASSERT_TRUE(log.GetEncodedLog(WriteInto(&encoded, size), size));

  ASSERT_EQ(expected_output, encoded);
}

namespace {

class NoTimeMetricsLog : public MetricsLog {
 public:
  NoTimeMetricsLog(std::string client_id, int session_id):
      MetricsLog(client_id, session_id) {}
  virtual ~NoTimeMetricsLog() {}

  // Override this so that output is testable.
  virtual std::string GetCurrentTimeString() {
    return std::string();
  }
};

};  // namespace

TEST(MetricsLogTest, WindowEvent) {
  std::string expected_output =
    "<log clientid=\"bogus client ID\">\n"
    " <window action=\"create\" windowid=\"0\" session=\"0\" time=\"\"/>\n"
    " <window action=\"open\" windowid=\"1\" parent=\"0\" "
        "session=\"0\" time=\"\"/>\n"
    " <window action=\"close\" windowid=\"1\" parent=\"0\" "
        "session=\"0\" time=\"\"/>\n"
    " <window action=\"destroy\" windowid=\"0\" session=\"0\" time=\"\"/>\n"
    "</log>";

  NoTimeMetricsLog log("bogus client ID", 0);
  log.RecordWindowEvent(MetricsLog::WINDOW_CREATE, 0, -1);
  log.RecordWindowEvent(MetricsLog::WINDOW_OPEN, 1, 0);
  log.RecordWindowEvent(MetricsLog::WINDOW_CLOSE, 1, 0);
  log.RecordWindowEvent(MetricsLog::WINDOW_DESTROY, 0, -1);
  log.CloseLog();

  ASSERT_EQ(4, log.num_events());

  int size = log.GetEncodedLogSize();
  ASSERT_GT(size, 0);

  std::string encoded;
  ASSERT_TRUE(log.GetEncodedLog(WriteInto(&encoded, size), size));

  ASSERT_EQ(expected_output, encoded);
}

TEST(MetricsLogTest, LoadEvent) {
  std::string expected_output =
    "<log clientid=\"bogus client ID\">\n"
    " <document action=\"load\" docid=\"1\" window=\"3\" loadtime=\"7219\" "
    "origin=\"link\" "
    "session=\"0\" time=\"\"/>\n"
    "</log>";

  NoTimeMetricsLog log("bogus client ID", 0);
  log.RecordLoadEvent(3, GURL("http://google.com"), PageTransition::LINK,
                      1, TimeDelta::FromMilliseconds(7219));

  log.CloseLog();

  ASSERT_EQ(1, log.num_events());

  int size = log.GetEncodedLogSize();
  ASSERT_GT(size, 0);

  std::string encoded;
  ASSERT_TRUE(log.GetEncodedLog(WriteInto(&encoded, size), size));

  ASSERT_EQ(expected_output, encoded);
}

// Make sure our ID hashes are the same as what we see on the server side.
TEST(MetricsLogTest, CreateHash) {
  static const struct {
    std::string input;
    std::string output;
  } cases[] = {
    {"Back", "0x0557fa923dcee4d0"},
    {"Forward", "0x67d2f6740a8eaebf"},
    {"NewTab", "0x290eb683f96572f1"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    std::string hash_string = MetricsLog::CreateHash(cases[i].input);

    // Convert to hex string
    // We're only checking the first 8 bytes, because that's what
    // the metrics server uses.
    std::string hash_hex = "0x";
    for (size_t j = 0; j < 8; j++) {
      StringAppendF(&hash_hex, "%02x",
                    static_cast<uint8>(hash_string.data()[j]));
    }
    EXPECT_EQ(cases[i].output, hash_hex);
  }
};
