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


// Helper class to manage the process of uploading metrics.
#include "backend/serverconnectionmanager.h"
#include "statsreport/common/const_product.h"
#include "iobuffer/iobuffer-inl.h"
#include "statsreport/aggregator-posix-inl.h"
#include "statsreport/const-posix.h"
#include "statsreport/formatter.h"
#include "statsreport/uploader.h"


DECLARE_int32(metrics_aggregation_interval);
DECLARE_int32(metrics_upload_interval);
DECLARE_string(metrics_server_name);
DECLARE_int32(metrics_server_port);

using stats_report::Formatter;
using stats_report::MetricsAggregatorPosix;

namespace {

bool AggregateMetrics(MetricsAggregatorPosix *aggregator) {
  if (!aggregator->AggregateMetrics()) {
    LOG(WARNING) << "Metrics aggregation failed for reasons unknown";
    return false;
  }

  return true;
}


bool ReportMetrics(MetricsAggregatorPosix *aggregator,
                   const char* extra_url_data,
                   const char* user_agent,
                   uint32 interval) {
  Formatter formatter(PRODUCT_NAME_STRING, interval);
  aggregator->FormatMetrics(&formatter);

  return UploadMetrics(extra_url_data, user_agent, formatter.output());
}

}  // namespace

namespace stats_report {

bool AggregateMetrics() {
  MetricsAggregatorPosix aggregator(g_global_metrics);
  return ::AggregateMetrics(&aggregator);
}

// Returns:
//   true if metrics were uploaded successfully, false otherwise
//   Note: False does not necessarily mean an error, just that no metrics
//         were uploaded
bool AggregateAndReportMetrics(const char* extra_url_arguments,
                               const char* user_agent,
                               bool force_report) {
  StatsUploader stats_uploader;
  return TestableAggregateAndReportMetrics(extra_url_arguments, user_agent,
                                           force_report, &stats_uploader);
}
// Returns:
//   true if metrics were uploaded successfully, false otherwise
//   Note: False does not necessarily mean an error, just that no metrics
//         were uploaded
bool TestableAggregateAndReportMetrics(const char* extra_url_arguments,
                                       const char* user_agent,
                                       bool force_report,
                                       StatsUploader* stats_uploader) {
  // Open the store
  MetricsAggregatorPosix aggregator(g_global_metrics);

  int32 now = static_cast<int32>(time(NULL));

  // Retrieve the last transmission time
  int32 last_transmission_time;
  bool success = aggregator.GetValue(kLastTransmissionTimeValueName,
                                     &last_transmission_time);

  // if last transmission time is missing or at all hinky, then
  // let's wipe all info and start afresh.
  if (!success || last_transmission_time > now) {
    LOG(WARNING) << "Hinky or missing last transmission time, wiping stats";

    aggregator.ResetMetrics();

    success = aggregator.SetValue(kLastTransmissionTimeValueName, now);
    if (!success)
      LOG(ERROR) << "Unable to write last transmission value";

    // we just wiped everything, let's not waste any more time
    return false;
  }

  if (!::AggregateMetrics(&aggregator))
    return false;

  if (now - last_transmission_time >= kStatsUploadInterval) {
    bool report_result = ReportMetrics(&aggregator, extra_url_arguments,
                                       user_agent,
                                       now - last_transmission_time);
    if (report_result) {
      LOG(INFO) << "Stats upload successful, resetting metrics";

      aggregator.ResetMetrics();
    } else {
      LOG(WARNING) << "Stats upload failed";
    }

    // No matter what, wait another upload interval to try again. It's better
    // to report older stats than hammer on the stats server exactly when it's
    // failed.
    (void)aggregator.SetValue(kLastTransmissionTimeValueName, now);
    return report_result;
  }
  return false;
}

}  // namespace stats_report
