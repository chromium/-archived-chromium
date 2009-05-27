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

#include <atlbase.h>
#include <atlcom.h>
#include <atlsafe.h>
#include <time.h>
#include "base/logging.h"

#include "statsreport/uploader.h"
#include "statsreport/aggregator-win32.h"
#include "statsreport/const-win32.h"
#include "statsreport/persistent_iterator-win32.h"
#include "statsreport/formatter.h"
#include "statsreport/common/const_product.h"

namespace stats_report {
// TODO: Refactor to avoid cross platform code duplication.

bool AggregateMetrics() {
  using stats_report::MetricsAggregatorWin32;
  MetricsAggregatorWin32 aggregator(g_global_metrics, PRODUCT_NAME_STRING_WIDE);
  if (!aggregator.AggregateMetrics()) {
    DLOG(WARNING) << "Metrics aggregation failed for reasons unknown";
    return false;
  }

  return true;
}


static bool ReportMetrics(const char* extra_url_data,
                          const char* user_agent,
                          DWORD interval,
                          StatsUploader* stats_uploader) {
  PersistentMetricsIteratorWin32 it(PRODUCT_NAME_STRING_WIDE), end;
  Formatter formatter(CStringA(PRODUCT_NAME_STRING), interval);

  for (; it != end; ++it)
    formatter.AddMetric(*it);
  DLOG(INFO) << "formatter.output() = " << formatter.output();
  return stats_uploader->UploadMetrics(extra_url_data, user_agent,
                                       formatter.output());
}

void ResetPersistentMetrics(CRegKey *key) {
  key->DeleteValue(kLastTransmissionTimeValueName);
  key->DeleteSubKey(kCountsKeyName);
  key->DeleteSubKey(kTimingsKeyName);
  key->DeleteSubKey(kIntegersKeyName);
  key->DeleteSubKey(kBooleansKeyName);
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
  CString key_name;
  key_name.Format(kStatsKeyFormatString, PRODUCT_NAME_STRING_WIDE);

  CRegKey key;
  LONG err = key.Create(HKEY_CURRENT_USER, key_name);
  if (ERROR_SUCCESS != err) {
    DLOG(WARNING) << "Unable to open metrics key";
    return false;
  }

  DWORD now = static_cast<DWORD>(time(NULL));

  // Retrieve the last transmission time
  DWORD last_transmission_time;
  DWORD value_type;
  ULONG value_len = sizeof(last_transmission_time);
  err = key.QueryValue(kLastTransmissionTimeValueName, &value_type,
                       &last_transmission_time, &value_len);

  // if last transmission time is missing or at all hinky, then
  // let's wipe all info and start afresh.
  if (ERROR_SUCCESS != err || REG_DWORD != value_type ||
      sizeof(last_transmission_time) != value_len ||
      last_transmission_time > now) {
    DLOG(WARNING) << "Hinky or missing last transmission time, wiping stats";

    ResetPersistentMetrics(&key);

    err = key.SetValue(kLastTransmissionTimeValueName, REG_DWORD,
                       &now, sizeof(now));
    if (ERROR_SUCCESS != err) {
      DLOG(ERROR) << "Unable to write last transmission value, error "
                  << std::hex << err;
    }
    // Force a report of the stats so we get everything currently in there.
    force_report = true;

    // we just wiped everything, let's not waste any more time
    // return; <-- skipping this since we still want to aggregate
  }

  if (!AggregateMetrics()) {
    DLOG(INFO) << "AggregateMetrics returned false";
    return false;
  }

  DLOG(INFO) << "Last transimission time: " << last_transmission_time;
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

      ResetPersistentMetrics(&key);
    } else {
      DLOG(WARNING) << "Stats upload failed";
    }

    // No matter what, wait another upload interval to try again. It's better
    // to report older stats than hammer on the stats server exactly when it's
    // failed.
    err = key.SetValue(kLastTransmissionTimeValueName, REG_DWORD,
                       &now, sizeof(now));
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
