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
// including initializing logging object, aggregating metrics, and uploading
// metrics to the stats server.

#include <build/build_config.h>
#ifdef OS_WIN
#include <atlbase.h>
#include "statsreport/const-win32.h"
#endif
#include "core/cross/types.h"
#include "plugin/cross/plugin_logging.h"
#include "plugin/cross/plugin_metrics.h"
#include "statsreport/common/const_product.h"
#include "statsreport/metrics.h"
#include "statsreport/uploader.h"
namespace o3d {


HRESULT StringEscape(const CString& str_in,
                     bool segment_only,
                     CString* escaped_string);
HRESULT GetRegStringValue(bool is_machine_key,
                          const CString& relative_key_path,
                          const CString& value_name,
                          CString* value);

PluginLogging::PluginLogging() : timer_(new HighresTimer()),
                                 running_time_(0),
                                 prev_uptime_seconds_(0),
                                 prev_cputime_seconds_(0) {
  DLOG(INFO) << "Creating logger.";
  timer_->Start();
}

bool PluginLogging::UpdateLogging() {
  // If sufficient time has not passed since last aggregation, we can just
  // return until next time.
  if (timer_->GetElapsedMs() < kStatsAggregationIntervalMSec) return false;

  // Reset timer.
  timer_->Start();
  // We are not exiting just yet so pass false for that argument.
  // We don't have to force stats reporting, so pass false for forcing, too.
  return ProcessMetrics(false, false);
}

static uint64 ToSeconds(FILETIME time) {
  ULARGE_INTEGER t;
  t.u.HighPart = time.dwHighDateTime;
  t.u.LowPart = time.dwLowDateTime;

  return t.QuadPart / 10000000L;
}

void PluginLogging::RecordProcessTimes() {
  FILETIME creation_time, exit_time, kernel_time, user_time;
  if (!::GetProcessTimes(::GetCurrentProcess(), &creation_time, &exit_time,
                         &kernel_time, &user_time)) {
    return;
  }
  FILETIME now;
  ::GetSystemTimeAsFileTime(&now);
  uint64 uptime = ToSeconds(now) - ToSeconds(creation_time);
  uint64 additional_uptime = uptime - prev_uptime_seconds_;
  metric_uptime_seconds += additional_uptime;
  running_time_ += additional_uptime;
  prev_uptime_seconds_ = uptime;

  uint64 cputime = ToSeconds(kernel_time) + ToSeconds(user_time);
  metric_cpu_time_seconds += (cputime - prev_cputime_seconds_);
  prev_cputime_seconds_ = cputime;
}

bool PluginLogging::ProcessMetrics(const bool exiting,
                                   const bool force_report) {
  DLOG(INFO) << "ProcessMetrics()";
  // Grab incremental process times. This has to be done each time
  // around the loop since time passes between iterations.
  RecordProcessTimes();

  // This mutex protects the writing to the registry. This way,
  // if we have multiple instances attempting to aggregate at
  // once, they won't overwrite one another.
  CHandle mutex(::CreateMutexA(NULL, FALSE, kMetricsLockName));
  if (NULL == mutex.m_h) {
    DLOG(WARNING) << "Unable to create metrics mutex";
    return false;
  }

  // If we can't get the mutex in 3 seconds, let's go around again.
  DWORD wait_result = ::WaitForSingleObject(mutex, 3000);
  if (WAIT_OBJECT_0 != wait_result) {
    DLOG(WARNING) << "Unable to get metrics mutex, error "
                  << std::hex << wait_result;
    return false;
  }
  if (exiting) {
    // If we're exiting, we aggregate to make sure that we record
    // the tail activity for posterity. We don't report, because
    // that might delay the process exit.
    // We also make sure to add a sample to the total running time.
    metric_running_time_seconds.AddSample(running_time_);
    DoAggregateMetrics();
  } else {
    CString user_id;
    if (FAILED(GetRegStringValue(true,  // is_machine_key
                                 CString(kRelativeGoopdateRegPath),
                                 CString(kRegValueUserId),
                                 &user_id))) {
      user_id = CString("unknown user_id");
    }
    CString user_id_escaped;
    StringEscape(user_id, true, &user_id_escaped);
    CStringA client_id_argument = CString("ui=")
                                  + user_id_escaped.GetString();
    DLOG(INFO) << "client id " << client_id_argument;

    std::string user_agent8 = std::string(kUserAgent) +
                              PRODUCT_VERSION_STRING;
    DoAggregateAndReportMetrics(client_id_argument,
                                user_agent8.c_str(), force_report);
  }

  ::ReleaseMutex(mutex);

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
  // This eturns true if metrics were uploaded.
  return stats_report::AggregateAndReportMetrics(extra_url_arguments,
                                                 user_agent,
                                                 force_report);
}

// This method is used for testing.
void PluginLogging::SetTimer(HighresTimer* timer) {
  timer_.reset(timer);
}

// Reads the specified string value from the specified registry key.
// Only supports value types REG_SZ and REG_EXPAND_SZ.
// REG_EXPAND_SZ strings are not expanded.
HRESULT GetRegStringValue(bool is_machine_key,
                          const CString& relative_key_path,
                          const CString& value_name,
                          CString* value) {
  if (!value) {
    return E_INVALIDARG;
  }

  value->Empty();
  HKEY root_key = is_machine_key ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  HKEY key = NULL;
  LONG res = ::RegOpenKeyEx(root_key, relative_key_path, 0, KEY_READ, &key);
  if (res != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(res);
  }

  // First get the size of the string buffer.
  DWORD type = 0;
  DWORD byte_count = 0;
  res = ::RegQueryValueEx(key, value_name, NULL, &type, NULL, &byte_count);
  if (ERROR_SUCCESS != res) {
    return HRESULT_FROM_WIN32(res);
  }
  if ((type != REG_SZ && type != REG_EXPAND_SZ) || (0 == byte_count)) {
    return E_FAIL;
  }

  CString local_value;
  // GetBuffer throws when not able to allocate the requested buffer.
  TCHAR* buffer = local_value.GetBuffer(byte_count / sizeof(TCHAR));
  res = ::RegQueryValueEx(key,
                          value_name,
                          NULL,
                          NULL,
                          reinterpret_cast<byte*>(buffer),
                          &byte_count);
  if (ERROR_SUCCESS == res) {
    local_value.ReleaseBufferSetLength(byte_count / sizeof(TCHAR));
    *value = local_value;
  }

  return HRESULT_FROM_WIN32(res);
}

HRESULT StringEscape(const CString& str_in,
                     bool segment_only,
                     CString* escaped_string) {
  if (!escaped_string) {
    return E_INVALIDARG;
  }

  DWORD buf_len = INTERNET_MAX_URL_LENGTH + 1;
  HRESULT hr = ::UrlEscape(str_in,
                           escaped_string->GetBufferSetLength(buf_len),
                           &buf_len,
                           segment_only ?
                           URL_ESCAPE_PERCENT | URL_ESCAPE_SEGMENT_ONLY :
                           URL_ESCAPE_PERCENT);
  if (SUCCEEDED(hr)) {
    escaped_string->ReleaseBuffer();
  }
  return hr;
}

PluginLogging* PluginLogging::InitializeUsageStatsLogging() {
  HKEY hkcu;
  ::RegOpenKeyEx(HKEY_CURRENT_USER,
                 kClientstateRegistryKey,
                 0,
                 KEY_SET_VALUE,
                 &hkcu);
  ::RegSetValueEx(hkcu, L"dr", 0, REG_SZ, (LPBYTE)"1\0", 2);
  ::RegCloseKey(hkcu);

  bool opt_in = GetOptInKeyValue(kClientstateRegistryKey, kOptInRegistryKey);

  return CreateUsageStatsLogger<PluginLogging>(opt_in);
}

bool PluginLogging::GetOptInKeyValue(const wchar_t* clientstate_registry_key,
                                     const wchar_t* opt_in_registry_key) {
#ifdef NDEBUG
  HKEY hkcu_opt;
  if (::RegOpenKeyEx(HKEY_CURRENT_USER,
                     clientstate_registry_key,
                     0,
                     KEY_QUERY_VALUE,
                     &hkcu_opt) != ERROR_SUCCESS) {
    return false;
  }
  DWORD opt_value;
  DWORD value_type;
  ULONG value_len = sizeof(opt_value);
  if (::RegQueryValueEx(hkcu_opt, opt_in_registry_key, 0,
                        &value_type, reinterpret_cast<BYTE *>(&opt_value),
                        &value_len) != ERROR_SUCCESS) {
    return false;
  }
  ::RegCloseKey(hkcu_opt);

  return opt_value == 1;
#else
  // If we are debugging, always return true.
  return true;
#endif
  // Default to opt-out for situations we don't handle.
  return false;
}

void PluginLogging::ClearLogs() {
  CRegKey key;
  CString key_name;
  key_name.Format(stats_report::kStatsKeyFormatString,
                  PRODUCT_NAME_STRING_WIDE);
  LONG err = key.Create(HKEY_CURRENT_USER, key_name);
  if (ERROR_SUCCESS != err) {
    DLOG(WARNING) << "Unable to open metrics key";
  }
  stats_report::ResetPersistentMetrics(&key);
}

}  // namespace o3d
