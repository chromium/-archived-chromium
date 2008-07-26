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

#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/metrics_log.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  for (int i = 0; i < arraysize(cases); i++) {
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
