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


// This file contains code to perform the necessary logging operations
// including:
// - initializing logging object
// - aggregating metrics
// - uploading metrics to the stats server

#include <build/build_config.h>
#include "statsreport/const-mac.h"
#include "core/cross/types.h"
#include "statsreport/metrics.h"
#include "plugin/cross/plugin_logging.h"
#include "plugin/cross/plugin_metrics.h"
#include "statsreport/common/const_product.h"
#include "statsreport/uploader.h"

namespace o3d {


#define kPrefString @"O3D_STATS"
#define kUserKey @"Google_O3D_User"


PluginLogging::PluginLogging() : timer_(new HighresTimer()),
                                 running_time_(0),
                                 prev_uptime_seconds_(0),
                                 prev_cputime_seconds_(0) {
  DLOG(INFO) << "Creating logger.";
  timer_->Start();
}

bool PluginLogging::UpdateLogging() {
  // Check that sufficient time has passed since last aggregation
  // Otherwise we can just return
  if (timer_->GetElapsedMs() < kStatsAggregationIntervalMSec) return false;

  // Sufficient time has passed, let's process
  // Reset timer
  timer_->Start();
  // We are not exiting just yet so pass false for that argument
  // And we don't have to force it, so pass false for forcing
  return ProcessMetrics(false, false);
}



void PluginLogging::RecordProcessTimes() {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  ProcessSerialNumber myProcess = {0, kCurrentProcess};

  NSDictionary* processDict = (NSDictionary*)ProcessInformationCopyDictionary(
      &myProcess,
      kProcessDictionaryIncludeAllInformationMask);

  ProcessInfoExtendedRec info;
  bzero(&info, sizeof(info));
  GetProcessInformation(&myProcess, (ProcessInfoRec*)&info);

  NSDate* launchDate = [processDict objectForKey:@"LSLaunchTime"];
  NSDate* nowDate = [NSDate dateWithTimeIntervalSinceNow:0];
  NSTimeInterval uptime = [nowDate timeIntervalSinceDate:launchDate];

  uint64 additional_uptime = uptime - prev_uptime_seconds_;
  metric_uptime_seconds += additional_uptime;
  running_time_ += additional_uptime;
  prev_uptime_seconds_ = uptime;

  uint64 cputime = ((double)info.processActiveTime) / 60.0;
  metric_cpu_time_seconds += (cputime - prev_cputime_seconds_);
  prev_cputime_seconds_ = cputime;

  [pool release];
}

static NSString* generateGUID() {
  CFUUIDRef uuidRef = CFUUIDCreate(kCFAllocatorDefault);
  NSString* returned = (NSString*)CFUUIDCreateString(kCFAllocatorDefault,
                                                     uuidRef);
  CFRelease(uuidRef);
  return returned;
}


// Read a pref string for current user, but global to all apps.
static NSString* ReadGlobalPreferenceString(NSString *key) {
  return (NSString *)CFPreferencesCopyValue((CFStringRef)key,
                                            kCFPreferencesAnyApplication,
                                            kCFPreferencesCurrentUser,
                                            kCFPreferencesCurrentHost);
}


// Write a pref string for the current user, but global to all apps.
static void WriteGlobalPreferenceString(NSString *key,
                                        NSString *value) {
  CFPreferencesSetValue((CFStringRef)key,
                        (CFPropertyListRef)value,
                        kCFPreferencesAnyApplication,
                        kCFPreferencesCurrentUser,
                        kCFPreferencesCurrentHost);

  CFPreferencesSynchronize(kCFPreferencesAnyApplication,
                           kCFPreferencesCurrentUser,
                           kCFPreferencesCurrentHost);
}


static const char *GetUserID(void) {
  static NSString* id_str = NULL;

  if (id_str == NULL) {
    // if not in cache, try to read it
    id_str = ReadGlobalPreferenceString(kUserKey);

    // if it was not stored, then create and store it
    if (!id_str) {
      id_str = generateGUID();
      WriteGlobalPreferenceString(kUserKey, id_str);
    }
  }
  return [id_str UTF8String];
}


bool PluginLogging::ProcessMetrics(const bool exiting,
                                   const bool force_report) {
  DLOG(INFO) << "ProcessMetrics()";
  // Grab incremental process times, has to be done each time
  // around the loop, because - well - time passes between
  // iterations :)
  RecordProcessTimes();

  // This mutex protects the writing to the registry. This way,
  // if we have multiple instances attempting to aggregate at
  // once, they won't overwrite one another.

  // 1. return if we can't get mutex


  if (exiting) {
    // If we're exiting, we aggregate to make sure that we record
    // the tail activity for posterity. We don't report, because
    // that might delay the process exit arbitrarily, and we don't
    // want that.
    // We also make sure to add a sample to the running time
    metric_running_time_seconds.AddSample(running_time_);
    DoAggregateMetrics();
  } else {
    // TODO: placeholder - where is this supposed to come from?
    std::string user_id(GetUserID());
    std::string ui_param("ui=");
    std::string client_id_argument = ui_param + user_id;
    std::string user_agent8 = std::string(kUserAgent) +
                              PRODUCT_VERSION_STRING;
    DoAggregateAndReportMetrics(client_id_argument.c_str(),
                                user_agent8.c_str(), force_report);
  }


  // 2. release mutex

  return true;
}

void PluginLogging::DoAggregateMetrics() {
  DLOG(INFO) << "DoAggregateMetrics()";
  stats_report::AggregateMetrics();
}

bool PluginLogging::DoAggregateAndReportMetrics(
    const char* extra_url_arguments,
    const char* user_agent,
    const bool force_report) {
  DLOG(INFO) << "DoAggregateAndReportMetrics()";
  // AggregateAndReportMetrics returns true if metrics were uploaded
  return stats_report::AggregateAndReportMetrics(extra_url_arguments,
                                                 user_agent,
                                                 force_report);
}

void PluginLogging::SetTimer(HighresTimer* timer) {
  timer_.reset(timer);
}


PluginLogging* PluginLogging::InitializeUsageStatsLogging() {
  bool opt_in = GetOptInKeyValue();

  return CreateUsageStatsLogger<PluginLogging>(opt_in);
}

bool PluginLogging::GetOptInKeyValue(void) {
#ifdef NDEBUG
  NSString *value = ReadGlobalPreferenceString(kPrefString);
  return ![value isEqualToString:@"NO"];
#else
  return true;
#endif
}

void PluginLogging::ClearLogs() {
  stats_report::ResetPersistentMetrics();
}

}  // namespace o3d
