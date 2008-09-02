// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/time.h"

#define USE_UNRELIABLE_NOW

namespace base {

static const char* kEventTypeNames[] = {
  "BEGIN",
  "END",
  "INSTANT"
};

static const wchar_t* kLogFileName = L"trace_%d.log";

TraceLog::TraceLog() : enabled_(false), log_file_(NULL) {
}

TraceLog::~TraceLog() {
  Stop();
}

// static
bool TraceLog::IsTracing() {
  TraceLog* trace = Singleton<TraceLog>::get();
  return trace->enabled_;
}

// static
bool TraceLog::StartTracing() {
  TraceLog* trace = Singleton<TraceLog>::get();
  return trace->Start();
}

bool TraceLog::Start() {
  if (enabled_)
    return true;
  enabled_ = OpenLogFile();
  if (enabled_)
    trace_start_time_ = TimeTicks::Now();
  return enabled_;
}

// static
void TraceLog::StopTracing() {
  TraceLog* trace = Singleton<TraceLog>::get();
  return trace->Stop();
}

void TraceLog::Stop() {
  if (enabled_) {
    enabled_ = false;
    CloseLogFile();
  }
}

void TraceLog::CloseLogFile() {
  if (log_file_) {
#if defined(OS_WIN)
    ::CloseHandle(log_file_);
#elif defined(OS_POSIX)
    fclose(log_file_);
#endif
  }
}

bool TraceLog::OpenLogFile() {
  std::wstring pid_filename =
    StringPrintf(kLogFileName, process_util::GetCurrentProcId());
  std::wstring log_file_name;
  PathService::Get(base::DIR_EXE, &log_file_name);
  file_util::AppendToPath(&log_file_name, pid_filename);
#if defined(OS_WIN)
  log_file_ = ::CreateFile(log_file_name.c_str(), GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (log_file_ == INVALID_HANDLE_VALUE || log_file_ == NULL) {
    // try the current directory
    log_file_ = ::CreateFile(pid_filename.c_str(), GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (log_file_ == INVALID_HANDLE_VALUE || log_file_ == NULL) {
      log_file_ = NULL;
      return false;
    }
  }
  ::SetFilePointer(log_file_, 0, 0, FILE_END);
#elif defined(OS_POSIX)
  log_file_ = fopen(WideToUTF8(log_file_name).c_str(), "a");
  if (log_file_ == NULL)
    return false;
#endif
  return true;
}

void TraceLog::Trace(const std::string& name, 
                     EventType type,
                     void* id,
                     const std::wstring& extra,
                     const char* file, 
                     int line) {
  if (!enabled_)
    return;
  Trace(name, type, id, WideToUTF8(extra), file, line);
}

void TraceLog::Trace(const std::string& name, 
                     EventType type,
                     void* id,
                     const std::string& extra,
                     const char* file, 
                     int line) {
  if (!enabled_)
    return;

#ifdef USE_UNRELIABLE_NOW
  TimeTicks tick = TimeTicks::UnreliableHighResNow();
#else
  TimeTicks tick = TimeTicks::Now();
#endif
  TimeDelta delta = tick - trace_start_time_;
  int64 usec = delta.InMicroseconds();
  std::string msg = 
    StringPrintf("%I64d 0x%lx:0x%lx %s %s [0x%lx %s] <%s:%d>\r\n",
                 usec,
                 process_util::GetCurrentProcId(),
                 PlatformThread::CurrentId(),
                 kEventTypeNames[type],
                 name.c_str(),
                 id,
                 extra.c_str(),
                 file, 
                 line);

  Log(msg);
}

void TraceLog::Log(const std::string& msg) {
  AutoLock lock(file_lock_);

#if defined (OS_WIN)
  SetFilePointer(log_file_, 0, 0, SEEK_END);
  DWORD num;
  WriteFile(log_file_, (void*)msg.c_str(), (DWORD)msg.length(), &num, NULL);
#elif defined (OS_POSIX)
  fprintf(log_file_, "%s", msg.c_str());
#endif
}

} // namespace base

