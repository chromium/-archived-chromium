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

//
// Breakpad crash report uploader
// (adapted from code in Google Gears)
//


#include <map>
#include <windows.h>
#include <assert.h>
#include <shellapi.h>
#include <tchar.h>
#include <time.h>

#include "breakpad_config.h"
#include "client/windows/sender/crash_report_sender.h"

using std::map;
using std::wstring;
using google_breakpad::CrashReportSender;
using google_breakpad::ReportResult;

bool CanSendMinidump(const wchar_t *product_name);
void SendMinidump(const wchar_t *minidump_filename,
                  const wchar_t *product_name,
                  const wchar_t *product_version);


int _tmain(int argc, _TCHAR* argv[]) {
  SendMinidump(argv[1], argv[2], argv[3]);
  return 0;
}

bool CanSendMinidump(const wchar_t *product_name) {
  // useful for testing purposes
  if (kCrashReportAlwaysUpload) {
    return true;
  }

  bool can_send = false;

  time_t current_time;
  time(&current_time);

  // For throttling, we remember when the last N minidumps were sent.

  time_t past_send_times[kCrashReportsMaxPerInterval];
  DWORD bytes = sizeof(past_send_times);
  memset(&past_send_times, 0, bytes);

  HKEY reg_key;
  DWORD create_type;
  if (ERROR_SUCCESS != RegCreateKeyExW(HKEY_CURRENT_USER,
                                       kCrashReportThrottlingRegKey,
                                       0,
                                       NULL,
                                       0,
                                       KEY_READ | KEY_WRITE, NULL,
                                       &reg_key, &create_type)) {
    return false;  // this should never happen, but just in case
  }

  if (ERROR_SUCCESS != RegQueryValueEx(reg_key, product_name, NULL,
                                       NULL,
                                       reinterpret_cast<BYTE*>(past_send_times),
                                       &bytes)) {
    // this product hasn't sent any crash reports yet
    can_send = true;
  } else {
    // find crash reports within the last interval
    int crashes_in_last_interval = 0;
    for (int i = 0; i < kCrashReportsMaxPerInterval; ++i) {
      if (current_time - past_send_times[i] < kCrashReportsIntervalSeconds) {
        ++crashes_in_last_interval;
      }
    }

    can_send = crashes_in_last_interval < kCrashReportsMaxPerInterval;
  }

  if (can_send) {
    memmove(&past_send_times[1],
            &past_send_times[0],
            sizeof(time_t) * (kCrashReportsMaxPerInterval - 1));
    past_send_times[0] = current_time;
  }

  RegSetValueEx(reg_key, product_name, 0, REG_BINARY,
                reinterpret_cast<BYTE*>(past_send_times),
                sizeof(past_send_times));

  return can_send;
}

void SendMinidump(const wchar_t *minidump_filename,
                  const wchar_t *product_name,
                  const wchar_t *product_version) {
  if (CanSendMinidump(product_name)) {
    map<std::wstring, std::wstring> parameters;
    parameters[kCrashReportProductParam] = product_name;
    parameters[kCrashReportVersionParam] = std::wstring(product_version);

    std::wstring minidump_wstr(minidump_filename);

    CrashReportSender sender(L"");

    for (int i = 0; i < kCrashReportAttempts; ++i) {
      wstring upload_result;
      ReportResult result = sender.SendCrashReport(kCrashReportUrl,
                                                   parameters,
                                                   minidump_wstr,
                                                   &upload_result);
      if (result == google_breakpad::RESULT_FAILED) {
        Sleep(kCrashReportResendPeriodMs);
      } else {
        // RESULT_SUCCEEDED or RESULT_REJECTED
        break;
      }
    }
  }

  DeleteFileW(minidump_filename);
}
