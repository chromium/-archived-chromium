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


#include "plugin/cross/plugin_logging.h"
#include <shlwapi.h>
#ifdef OS_WIN
#include <atlbase.h>
#endif
#include "statsreport/metrics.h"
#include "statsreport/uploader.h"
#ifdef OS_WIN
#include "tests/common/win/testing_common.h"
#endif

// This mock class mimics the timer so that we don't actually have to wait for
// the timer to timeout in the tests since we're not testing the timer here.
class MockTimer : public HighresTimer {
 public:
  using HighresTimer::Start;
  MockTimer() : elapsed_ms_(0) {}
  virtual ULONGLONG GetElapsedMs() const;
  virtual void SetElapsedMs(const ULONGLONG ms);

 private:
  ULONGLONG elapsed_ms_;
};

ULONGLONG MockTimer::GetElapsedMs() const {
  return elapsed_ms_;
}
void MockTimer::SetElapsedMs(const ULONGLONG ms) {
  elapsed_ms_ = ms;
}

// This mock class mocks the stats uploader so we don't a) log false stats
// and b) send hits to the server every time we run the tests.
class MockStatsUploader : public stats_report::StatsUploader {
 public:
  MockStatsUploader() {}
  ~MockStatsUploader() {}
  virtual bool UploadMetrics(const char* extra_url_data,
                             const char* user_agent,
                             const char *content);
};

bool MockStatsUploader::UploadMetrics(const char* extra_url_data,
                                      const char* user_agent,
                                      const char *content) {
  // Return true to indicate that they were uploaded successfully.
  return true;
}

// This subclass is here just to make the protected methods public, so
// the testing functions can call them.
class MockPluginLogging : public o3d::PluginLogging {
 public:
  using o3d::PluginLogging::UpdateLogging;
  using o3d::PluginLogging::SetTimer;
  MockPluginLogging() : o3d::PluginLogging(),
                        aggregate_metrics_success_(0),
                        upload_metrics_success_(false) {}
  virtual bool ProcessMetrics(const bool exiting, const bool force_report);
  virtual void DoAggregateMetrics();
  virtual bool DoAggregateAndReportMetrics(const char* extra_url_arguments,
                                           const char* user_agent,
                                           const bool force_report);
  void SetAggregateMetricsSuccess(const int value);
  int AggregateMetricsSuccess();
  int UploadMetricsSuccess();
 private:
  int aggregate_metrics_success_;
  bool upload_metrics_success_;
};

bool MockPluginLogging::ProcessMetrics(const bool exiting,
                                      const bool force_report) {
  return true;
}

void MockPluginLogging::DoAggregateMetrics() {
  aggregate_metrics_success_ = 1;
}

bool MockPluginLogging::DoAggregateAndReportMetrics(
    const char* extra_url_arguments,
    const char* user_agent,
    const bool force_report) {
  aggregate_metrics_success_ = 2;
  MockStatsUploader stats_uploader;
  upload_metrics_success_ = TestableAggregateAndReportMetrics(
      extra_url_arguments,
      user_agent,
      force_report,
      &stats_uploader);
  return upload_metrics_success_;
}

void MockPluginLogging::SetAggregateMetricsSuccess(const int value) {
  aggregate_metrics_success_ = value;
}

int MockPluginLogging::AggregateMetricsSuccess() {
  return aggregate_metrics_success_;
}

int MockPluginLogging::UploadMetricsSuccess() {
  return upload_metrics_success_;
}

// Test fixture for the PluginLogging tests.
class PluginLoggingTests : public testing::Test {
 protected:
  virtual void SetUp();
  virtual void TearDown();

  MockPluginLogging* plugin_logging() { return plugin_logging_; }
  void SetPluginLogging(MockPluginLogging* plugin_logging_in) {
    plugin_logging_ = plugin_logging_in;
  }
  MockTimer* mock_timer() { return mock_timer_; }

 private:
  MockPluginLogging* plugin_logging_;
  MockTimer* mock_timer_;
};

void PluginLoggingTests::SetUp() {
  HRESULT hr = CoInitialize(NULL);
  mock_timer_ = new MockTimer();
  plugin_logging_ = new MockPluginLogging();
  plugin_logging_->SetTimer(mock_timer_);
  stats_report::g_global_metrics.Initialize();
}

void PluginLoggingTests::TearDown() {
  // Only unitialize if a plugin_logging_ exists.
  // If it does not exist, it means we also never initialized.
  if (plugin_logging_) {
    delete plugin_logging_;
    stats_report::g_global_metrics.Uninitialize();
  }
}

// Test if the metric collection is properly initialized.
TEST_F(PluginLoggingTests, InitializeMetricCollection) {
  EXPECT_TRUE(stats_report::g_global_metrics.initialized());
}

// Tests the PluginLogging's metric processing functions.
TEST_F(PluginLoggingTests, ProcessMetricsTests) {
  EXPECT_FALSE(plugin_logging()->UpdateLogging());
  // NOTE: This time should be greater than the desired time.
  mock_timer()->SetElapsedMs(5 * 60 * 1000);
  EXPECT_TRUE(plugin_logging()->UpdateLogging());

  // This time should be less than the interval.
  mock_timer()->SetElapsedMs(1000);
  EXPECT_FALSE(plugin_logging()->UpdateLogging());
}

// Tests that the proper method to aggregate metrics was called.
TEST_F(PluginLoggingTests, AggregateMetricsTests) {
  // Start by initializing the variable which tells us success or not.
  plugin_logging()->SetAggregateMetricsSuccess(0);
  // Pass false for "exiting" parameter to call AggregateMetrics.
  // Value does not matter for "force_report" for this testt so pass false.
  EXPECT_TRUE(plugin_logging()->PluginLogging::ProcessMetrics(true, false));
  EXPECT_EQ(plugin_logging()->AggregateMetricsSuccess(), 1);

  plugin_logging()->SetAggregateMetricsSuccess(0);
  // Pass true for "exiting" parameter to call AggregateAndReportMetrics.
  // Value does not matter for "force_report" for this testt so pass false.
  EXPECT_TRUE(plugin_logging()->PluginLogging::ProcessMetrics(false, false));
  EXPECT_EQ(plugin_logging()->AggregateMetricsSuccess(), 2);
}

// Check that the force_report boolean forces reporting of the metrics.
TEST_F(PluginLoggingTests, CheckForceReport) {
  // Using this call rather than just calling TestableAggregateAndReportMetrics
  // directly because this is the stand alone call that we want to test.
  // Pass false for "exiting" since reporting does not happen otherwise.
  EXPECT_TRUE(plugin_logging()->PluginLogging::ProcessMetrics(false, true));
}

// Tests that when opt_in is turned on we create a logger and process metrics.
TEST_F(PluginLoggingTests, CheckOptIn) {
  TearDown();
  SetPluginLogging(
      o3d::PluginLogging::CreateUsageStatsLogger<MockPluginLogging>(true));
  EXPECT_TRUE(plugin_logging());
}

// Tests that when opt_in is turned OFF we do not create a logger.
TEST_F(PluginLoggingTests, CheckOptOut) {
  TearDown();
  SetPluginLogging(
      o3d::PluginLogging::CreateUsageStatsLogger<MockPluginLogging>(false));
  EXPECT_FALSE(plugin_logging());
}
