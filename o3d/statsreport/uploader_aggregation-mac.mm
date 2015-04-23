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
#include "statsreport/aggregator-mac.h"
#include "statsreport/common/const_product.h"
#include "statsreport/const-mac.h"
#include "statsreport/formatter.h"
#include "statsreport/uploader.h"


using stats_report::Formatter;
using stats_report::MetricsAggregatorMac;

namespace stats_report {

bool AggregateMetrics() {
  using stats_report::MetricsAggregatorMac;
  MetricsAggregatorMac aggregator(g_global_metrics);
  if (!aggregator.AggregateMetrics()) {
    DLOG(WARNING) << "Metrics aggregation failed for reasons unknown";
    return false;
  }

  return true;
}


static bool ReportMetrics(const char* extra_url_data,
                          const char* user_agent,
                          uint32 interval,
                          StatsUploader* stats_uploader) {
  NSDictionary *dict =
      [NSDictionary dictionaryWithContentsOfFile:O3DStatsPath()];

  if (dict == nil)
    return false;

  Formatter formatter(PRODUCT_NAME_STRING, interval);
  NSArray *keys = [dict allKeys];
  int count = [keys count];

  for(int x = 0; x < count; ++x) {
    NSString *name = [keys objectAtIndex:x];
    NSString *short_name = [name substringFromIndex:2];

    if ([short_name length] == 0)
      continue;

    const char *short_name_c_str = [short_name UTF8String];
    id item = [dict objectForKey:name];
    MetricBase *current = NULL;
    switch([name characterAtIndex:0]) {
      case 'C':
        current = new CountMetric(short_name_c_str,
                                  [item longLongValue]);
        break;
      case 'B':
        current = new BoolMetric(short_name_c_str,
                                 [item boolValue] == YES);
        break;
      case 'T': {
        TimingMetric::TimingData data;
        data.count = [[item objectAtIndex:0] intValue];
        data.sum = [[item objectAtIndex:1] longLongValue];
        data.minimum = [[item objectAtIndex:2] longLongValue];
        data.maximum = [[item objectAtIndex:3] longLongValue];
        current = new TimingMetric(short_name_c_str, data);
        }
        break;
      case 'I':
        current = new IntegerMetric(short_name_c_str,
                                    [item longLongValue]);
        break;
      // We don't send this piece of data as it's not a metric.
      case 'L': // "LastTransmission"
        break;
    }
    if (current != NULL)
      formatter.AddMetric(current);
  }

  DLOG(INFO) << "formatter.output() = " << formatter.output();
  return stats_uploader->UploadMetrics(extra_url_data,
                                       user_agent,
                                       formatter.output());
}

void ResetPersistentMetrics() {
  NSError *error = nil;
  /*
  [[NSFileManager defaultManager] removeItemAtPath:O3DStatsPath()
                                             error:&error];
                                             */
  [[NSFileManager defaultManager] removeFileAtPath:O3DStatsPath()
                                           handler:nil];
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

static int GetLastTransmissionTime() {
  NSDictionary *dict =
      [NSDictionary dictionaryWithContentsOfFile:O3DStatsPath()];

  if (dict == nil)
    return 0;

  NSNumber *when = [dict objectForKey:kLastTransmissionTimeValueName];

  if (when == nil)
    return 0;

  return [when intValue];
}

static void SetLastTransmissionTime(int when) {
  NSMutableDictionary *dict =
      [NSMutableDictionary dictionaryWithContentsOfFile:O3DStatsPath()];

  if (dict == nil)
    dict = [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:when]
                                       forKey:kLastTransmissionTimeValueName];
  else
    [dict setObject:[NSNumber numberWithInt:when]
             forKey:kLastTransmissionTimeValueName];

  [dict writeToFile:O3DStatsPath() atomically:YES];
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
  MetricsAggregatorMac aggregator(g_global_metrics);

  int32 now = static_cast<int32>(time(NULL));

  // Retrieve the last transmission time
  int32 last_transmission_time = GetLastTransmissionTime();

  // if last transmission time is missing or at all dodgy, then
  // let's wipe all info and start afresh.
  if (last_transmission_time == 0 || last_transmission_time > now) {
    LOG(WARNING) << "dodgy or missing last transmission time, wiping stats";

    ResetPersistentMetrics();

    SetLastTransmissionTime(now);

    // Force a report of the stats so we get everything currently in there.
    force_report = true;
  }

  if (!AggregateMetrics()) {
    DLOG(INFO) << "AggregateMetrics returned false";
    return false;
  }

  DLOG(INFO) << "Last transmission time: " << last_transmission_time;
  DLOG(INFO) << "Now: " << now;
  DLOG(INFO) << "Now - Last transmission time: "
  << now - last_transmission_time;
  DLOG(INFO) << "Compared to: " << kStatsUploadIntervalSec;

  // Set last_transmission_time such that it will force
  // an upload of the metrics
  if (force_report) {
    last_transmission_time = now - kStatsUploadIntervalSec;
  }
  if (now - last_transmission_time >= kStatsUploadIntervalSec) {
    bool report_result = ReportMetrics(extra_url_arguments, user_agent,
                                       now - last_transmission_time,
                                       stats_uploader);
    if (report_result) {
      DLOG(INFO) << "Stats upload successful, resetting metrics";

      ResetPersistentMetrics();
    } else {
      DLOG(WARNING) << "Stats upload failed";
    }

    // No matter what, wait another upload interval to try again. It's better
    // to report older stats than hammer on the stats server exactly when it
    // has failed.
    SetLastTransmissionTime(now);
    return report_result;
  }
  return false;
}

// Used primarily for testing. Default functionality.
bool StatsUploader::UploadMetrics(const char* extra_url_data,
                                  const char* user_agent,
                                  const char *content) {
  return stats_report::UploadMetrics(extra_url_data, user_agent, content);
}


}  // namespace stats_report
