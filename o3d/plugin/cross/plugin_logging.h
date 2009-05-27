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


// This file defines the logging object which performs the metric aggregation
// and uploading. This class takes care of the initialization of the logging
// object and determining if the user has opted in or out to having logs sent
// back. Furthermore, there are some helper functions to make testing easier.

#ifndef O3D_PLUGIN_CROSS_PLUGIN_LOGGING_H_
#define O3D_PLUGIN_CROSS_PLUGIN_LOGGING_H_

#include "statsreport/common/highres_timer.h"
#include "base/scoped_ptr.h"
#include "base/basictypes.h"

namespace o3d {

class PluginLogging {
 public:
  PluginLogging();
  virtual ~PluginLogging() {}

  // Check to see if sufficient time has passed to process metrics. If such
  // time has passed, we reset the timer and proceed with processing metrics.
  //
  // Returns true if the metrics were processed properly.
  bool UpdateLogging();

  // Record how much time the plugin has spent running.
  virtual void RecordProcessTimes();

  // Takes care of gathering current statistics and uploading them to the server
  // if necessary.
  //
  // Parameters:
  //   exiting - whether the program is exiting
  //
  // Returns true if metrics were uploaded and/or aggregated successfully.
  virtual bool ProcessMetrics(const bool exiting,
                              const bool force_report);

  // A helper function to call AggregateMetrics used for testing. Calls
  // AggregateMetrics which gathers up the current metrics, puts them in
  // the registry and then resets them.
  virtual void DoAggregateMetrics();

  // A helper function for testing. This function calls
  // stats_report::AggregateAndReportMetrics which will aggregate the metrics
  // and upload to the server if sufficient time has passed.
  //
  // Parameters:
  //   extra_url_argument - extra url to be added after source id (O3D)
  //                        and version number
  //   user_agent - eventually the client_id, currently not used
  //
  // Returns true if metrics were uploaded successfully. Note: false does
  // not necessarily mean an error; just that no metrics were uploaded.
  virtual bool DoAggregateAndReportMetrics(const char* extra_url_arguments,
                                           const char* user_agent,
                                           const bool force_report);

  // PluginLogging assumes ownership of the timer.
  void SetTimer(HighresTimer* timer);

  // Factory method for creating the logger and taking care of initialization
  // and checks for opt-in/out.
  //
  // Returns the results CreateUsageStatsLogger which will be a pointer to new
  // PluginLogging object if the user opted in or NULL if they opted out.
  //
  // The existence of a PluginLogging object is used to check if logging is
  // turned on in other parts of the code.
  static PluginLogging* InitializeUsageStatsLogging();

  // Access the key determing opt-in. Separated out for testing.
  // Returns true if the user opted in.
#ifdef OS_MACOSX
  static bool GetOptInKeyValue(void);
#else
  static bool GetOptInKeyValue(const wchar_t* clientstate_registry_key,
                               const wchar_t* opt_in_registry_key);
#endif

  // Method for actually creating the logger. Separated out for testing.
  //
  // Returns the results CreateUsageStatsLogger which will be a pointer to new
  // PluginLogging object if the user opted in or NULL if they opted out.
  //
  // The existence of a PluginLogging object is used to check if logging is
  // turned on in other parts of the code.
  template <class LoggingType>
  static LoggingType* CreateUsageStatsLogger(const bool opt_in) {
    if (opt_in == true) {
      // They opted in!
      LoggingType* logger = new LoggingType();
      stats_report::g_global_metrics.Initialize();

      // Do an initial grab of the metrics. Don't pass true for force_report.
      // This will force an upload of the metrics the first time o3d is run
      // since the lastTransmission metric will not exist.
      logger->ProcessMetrics(false, false);
      return logger;
    }
    // Otherwise, they opted out so we make sure the registry is clear
    ClearLogs();
    return NULL;
  }

  // Method for cleaning out the logs. Used if the user opts-out to make sure
  // we don't retain any information from them.
  static void ClearLogs();

 private:
  // Timer for determining the next time aggregation should occur.
  scoped_ptr<HighresTimer> timer_;
  uint64 running_time_;
  uint64 prev_uptime_seconds_;
  uint64 prev_cputime_seconds_;
  DISALLOW_COPY_AND_ASSIGN(PluginLogging);
};

}  // namespace o3d

#endif  // O3D_PLUGIN_CROSS_PLUGIN_LOGGING_H_
