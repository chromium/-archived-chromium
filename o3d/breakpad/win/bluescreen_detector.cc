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


// BluescreenDetector attempts to identify cases where the machine bluescreened
// and was caused by o3d.

#include "breakpad/win/bluescreen_detector.h"
#include "plugin/cross/plugin_logging.h"
#include "plugin/cross/plugin_metrics.h"

#ifdef OS_WIN
#include <windows.h>
#include <rpc.h>
#endif

extern o3d::PluginLogging* g_logger;

namespace o3d {

using std::vector;

#ifdef OS_WIN

const wchar_t *kMarkerFileSuffix = L"_bs";

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
BluescreenDetector::BluescreenDetector()
  : started_(false) {
  time_manager_ = new TimeManager();
  marker_file_manager_ = new MarkerFileManager(time_manager_);
  bluescreen_logger_ = new BluescreenLogger();
}
#endif  // OS_WIN

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
BluescreenDetector::~BluescreenDetector() {
  if (started_) {
    Stop();
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void BluescreenDetector::Start() {
  // Here we check if any marker files (from a previous session) were not
  // properly cleaned up.  If so, then a blue-screen may have been caused
  // by us and we'll log it.
  int num_bluescreens = marker_file_manager_->DetectStrayMarkerFiles();

  if (num_bluescreens > 0) {
    bluescreen_logger_->LogBluescreen(num_bluescreens);
  }

  // Create a marker file for this session - it will be removed when the plugin
  // unloads (or in the breakpad exception handler).  If a blue-screen happens,
  // then this file will not be deleted and we'll hopefully detect it the next
  // time around (in a call to DetectStrayMarkerFiles())
  marker_file_manager_->CreateMarkerFile();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void BluescreenDetector::Stop() {
  marker_file_manager_->RemoveMarkerFile();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// MarkerFileManager

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Base implementation shared by mock and actual sub-classes
int MarkerFileManagerInterface::DetectStrayMarkerFiles() {
  vector<MarkerFileInfo> marker_files;
  GetMarkerFileList(&marker_files);

  // Go through all marker files and look for ones which were created before
  // the machine was last booted
  int stray_file_count = 0;

  for (vector<MarkerFileInfo>::size_type i = 0;
       i < marker_files.size(); ++i) {
    const MarkerFileInfo &file_info = marker_files[i];

    if (time_manager_->IsMarkerFileOld(file_info)) {
      // We've found a marker file which was created before we last re-booted
      // This could signal a blue-screen which we caused

      // Clean it up, so we don't continue detecting it again and logging a
      // blue-screen more than once
      DeleteMarkerFile(file_info);
      ++stray_file_count;
      // don't break here, since we need to detect and delete all old ones
    }
  }

  return stray_file_count;
}

#ifdef OS_WIN

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MarkerFileManager::CreateMarkerFile() {
  if (marker_file_) {
    // error
    return;
  }

  std::wstring marker_dir = GetMarkerDirectory();
  std::wstring uuid_string = GetUUIDString();

  // format a complete file path for the marker file
  wchar_t fullpath[MAX_PATH];
  _snwprintf(fullpath, MAX_PATH, L"%s%s%s",
           marker_dir.c_str(),
           uuid_string.c_str(),
           kMarkerFileSuffix);

  marker_file_name_ = fullpath;

  // Note that the file is created with the attribute FILE_FLAG_DELETE_ON_CLOSE
  // so even if the process crashes, this file will get cleaned up.
  // It's only if a bluescreen occurs (or plug is pulled out of wall) that the
  // file will not be deleted (in theory)
  marker_file_ = CreateFileW(fullpath,
                             GENERIC_WRITE,
                             0,
                             NULL,
                             CREATE_NEW,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
                             NULL);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MarkerFileManager::RemoveMarkerFile() {
  if (marker_file_) {
    // Strictly speaking, we don't really need to delete the file here since
    // the system will do it for us since FILE_FLAG_DELETE_ON_CLOSE was used,
    // but let's do it just to be sure...
    CloseHandle(marker_file_);
    marker_file_ = NULL;
    DeleteFile(marker_file_name_.c_str());
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static MarkerFileInfo GetMarkerFileInfo(const WIN32_FIND_DATA &find_data,
                                        const std::wstring &marker_dir) {
  std::wstring file_name = find_data.cFileName;
  std::wstring full_pathname_w = marker_dir + file_name;
  String full_pathname = WideToUTF8(full_pathname_w);
  uint64 creation_time =
    TimeManager::FileTimeToUInt64(find_data.ftCreationTime);

  MarkerFileInfo file_info(full_pathname, creation_time);
  return file_info;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool MarkerFileManager::GetMarkerFileList(vector<MarkerFileInfo> *file_list) {
  // Search the marker directory for all files ending in kMarkerFileSuffix
  std::wstring marker_dir = GetMarkerDirectory();
  std::wstring search_string = marker_dir + L"*" + kMarkerFileSuffix;

  WIN32_FIND_DATA find_data;
  HANDLE h = ::FindFirstFile(search_string.c_str(), &find_data);

  if (h != INVALID_HANDLE_VALUE) {
    MarkerFileInfo file_info = GetMarkerFileInfo(find_data, marker_dir);
    file_list->push_back(file_info);

    BOOL file_valid = true;

    while (file_valid) {
      file_valid = ::FindNextFile(h, &find_data);
      if (file_valid) {
        MarkerFileInfo file_info = GetMarkerFileInfo(find_data, marker_dir);
        file_list->push_back(file_info);
      }
    }

    ::FindClose(h);
  } else {
    // This can happen if there are no marker files found
    return true;
  }

  return true;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void MarkerFileManager::DeleteMarkerFile(const MarkerFileInfo &file_info) {
  std::wstring filename_w = UTF8ToWide(file_info.GetName());
  ::DeleteFileW(filename_w.c_str());
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
const std::wstring MarkerFileManager::GetUUIDString() {
    // now generate a GUID
    UUID guid = {0};
    ::UuidCreate(&guid);

    // and format into a wide-string
    wchar_t guid_string[37];
    _snwprintf(
        guid_string, sizeof(guid_string) / sizeof(guid_string[0]),
        L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2],
        guid.Data4[3], guid.Data4[4], guid.Data4[5],
        guid.Data4[6], guid.Data4[7]);

    return guid_string;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
const std::wstring MarkerFileManager::GetMarkerDirectory() {
  wchar_t temp_path[MAX_PATH];
  if (!::GetTempPathW(MAX_PATH, temp_path)) {
    return L"c:\\windows\\temp\\";
  }
  return temp_path;
}

#endif  // OS_WIN

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// TimeManager

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool TimeManagerInterface::IsMarkerFileOld(const MarkerFileInfo &file_info) {
  uint64 creation_time = file_info.GetCreationTime();
  uint64 current_time = GetCurrentTime();

  if (current_time < creation_time) {
    // should never happen, but log error
    return false;  // something wrong has happened here
  }

  uint64 file_time_since_now = current_time - creation_time;
  uint64 up_time = GetUpTime();

  bool is_old = (file_time_since_now > up_time);
  return is_old;
}

#ifdef OS_WIN

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
uint64 TimeManager::FileTimeToUInt64(FILETIME time) {
  // FILETIME units are 100-nanosecond intervals
  ULARGE_INTEGER li;
  li.LowPart = time.dwLowDateTime;
  li.HighPart = time.dwHighDateTime;
  return li.QuadPart;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
uint64 TimeManager::GetCurrentTime() {
  SYSTEMTIME current_time;
  ::GetSystemTime(&current_time);

  FILETIME file_time;
  ::SystemTimeToFileTime(&current_time, &file_time);

  // Now convert to uint64...
  return FileTimeToUInt64(file_time);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// in units of 100-nanosecond intervals (FILETIME units)
uint64 TimeManager::GetUpTime() {
  // NOTE : It would have been easier to simply use GetTickCount(),
  // but it wraps around to zero after 49.7 days!  There is a GetTickCount64()
  // function but it's only available on Vista.
  // Using QueryPerformanceCounter() and QueryPerformanceFrequency() appears
  // to be the best alternative.

  uint64 tick_count64 = 0;
  LARGE_INTEGER count;
  LARGE_INTEGER frequency;
  if (QueryPerformanceCounter(&count) &&
      QueryPerformanceFrequency(&frequency)) {
      LONGLONG total_seconds = count.QuadPart / frequency.QuadPart;

      // convert to 100-nanosecond intervals
      const LONGLONG k100NanosecondIntervalsPerSecond = 1000*1000*10;
      tick_count64 = total_seconds * k100NanosecondIntervalsPerSecond;
  }

  return tick_count64;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// BluescreenLogger
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void BluescreenLogger::LogBluescreen(int num_bluescreens) {
  metric_bluescreens_total += num_bluescreens;
  // Make sure we write this out to the registry immediately in case we're
  // about to bluescreen again before the metrics timer fires!
  if (g_logger) g_logger->ProcessMetrics(false, true);
}
#endif  // OS_WIN

}  // namespace o3d
