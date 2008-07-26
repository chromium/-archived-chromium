// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <ctime>
#include <iomanip>
#include <cstring>
#include <windows.h>
#include <algorithm>
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/lock_impl.h"
#include "base/logging.h"

namespace logging {

bool g_enable_dcheck = false;

const char* const log_severity_names[LOG_NUM_SEVERITIES] = {
  "INFO", "WARNING", "ERROR", "FATAL" };

int min_log_level = 0;
LogLockingState lock_log_file = LOCK_LOG_FILE;
LoggingDestination logging_destination = LOG_ONLY_TO_FILE;

const int kMaxFilteredLogLevel = LOG_WARNING;
char* log_filter_prefix = NULL;

// which log file to use? This is initialized by InitLogging or
// will be lazily initialized to the default value when it is
// first needed.
wchar_t log_file_name[MAX_PATH] = { 0 };

// this file is lazily opened and the handle may be NULL
HANDLE log_file = NULL;

// what should be prepended to each message?
bool log_process_id = false;
bool log_thread_id = false;
bool log_timestamp = true;
bool log_tickcount = false;

// An assert handler override specified by the client to be called instead of
// the debug message dialog.
LogAssertHandlerFunction log_assert_handler = NULL;

// The lock is used if log file locking is false. It helps us avoid problems
// with multiple threads writing to the log file at the same time.  Use
// LockImpl directly instead of using Lock, because Lock makes logging calls.
static LockImpl* log_lock = NULL;

// When we don't use a lock, we are using a global mutex. We need to do this
// because LockFileEx is not thread safe.
HANDLE log_mutex = NULL;

// Called by logging functions to ensure that debug_file is initialized
// and can be used for writing. Returns false if the file could not be
// initialized. debug_file will be NULL in this case.
bool InitializeLogFileHandle() {
  if (log_file)
    return true;

  if (!log_file_name[0]) {
    // nobody has called InitLogging to specify a debug log file, so here we
    // initialize the log file name to the default
    GetModuleFileName(NULL, log_file_name, MAX_PATH);
    wchar_t* last_backslash = wcsrchr(log_file_name, '\\');
    if (last_backslash)
      last_backslash[1] = 0; // name now ends with the backslash
    wcscat_s(log_file_name, L"debug.log");
  }

  log_file = CreateFile(log_file_name, GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (log_file == INVALID_HANDLE_VALUE || log_file == NULL) {
    // try the current directory
    log_file = CreateFile(L".\\debug.log", GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (log_file == INVALID_HANDLE_VALUE || log_file == NULL) {
      log_file = NULL;
      return false;
    }
  }
  SetFilePointer(log_file, 0, 0, FILE_END);
  return true;
}

void InitLogMutex() {
  if (!log_mutex) {
    // \ is not a legal character in mutex names so we replace \ with /
    std::wstring safe_name(log_file_name);
    std::replace(safe_name.begin(), safe_name.end(), '\\', '/');
    std::wstring t(L"Global\\");
    t.append(safe_name);
    log_mutex = ::CreateMutex(NULL, FALSE, t.c_str());
  }
}

void InitLogging(const wchar_t* new_log_file, LoggingDestination logging_dest,
                 LogLockingState lock_log, OldFileDeletionState delete_old) {
  g_enable_dcheck = CommandLine().HasSwitch(switches::kEnableDCHECK);

  if (log_file) {
    // calling InitLogging twice or after some log call has already opened the
    // default log file will re-initialize to the new options
    CloseHandle(log_file);
    log_file = NULL;
  }

  lock_log_file = lock_log;
  logging_destination = logging_dest;

  // ignore file options if logging is disabled or only to system
  if (logging_destination == LOG_NONE ||
      logging_destination == LOG_ONLY_TO_SYSTEM_DEBUG_LOG)
    return;

  wcscpy_s(log_file_name, MAX_PATH, new_log_file);
  if (delete_old == DELETE_OLD_LOG_FILE)
    DeleteFile(log_file_name);

  if (lock_log_file == LOCK_LOG_FILE) {
    InitLogMutex();
  } else if (!log_lock) {
    log_lock = new LockImpl();
  }

  InitializeLogFileHandle();
}

void SetMinLogLevel(int level) {
  min_log_level = level;
}

int GetMinLogLevel() {
  return min_log_level;
}

void SetLogFilterPrefix(const char* filter)  {
  if (log_filter_prefix) {
    delete[] log_filter_prefix;
    log_filter_prefix = NULL;
  }

  if (filter) {
    size_t size = strlen(filter)+1;
    log_filter_prefix = new char[size];
    strcpy_s(log_filter_prefix, size, filter);
  }
}

void SetLogItems(bool enable_process_id, bool enable_thread_id,
                 bool enable_timestamp, bool enable_tickcount) {
  log_process_id = enable_process_id;
  log_thread_id = enable_thread_id;
  log_timestamp = enable_timestamp;
  log_tickcount = enable_tickcount;
}

void SetLogAssertHandler(LogAssertHandlerFunction handler) {
  log_assert_handler = handler;
}

// Displays a message box to the user with the error message in it. For
// Windows programs, it's possible that the message loop is messed up on
// a fatal error, and creating a MessageBox will cause that message loop
// to be run. Instead, we try to spawn another process that displays its
// command line. We look for "Debug Message.exe" in the same directory as
// the application. If it exists, we use it, otherwise, we use a regular
// message box.
void DisplayDebugMessage(const std::string& str) {
  if (str.empty())
    return;

  // look for the debug dialog program next to our application
  wchar_t prog_name[MAX_PATH];
  GetModuleFileNameW(NULL, prog_name, MAX_PATH);
  wchar_t* backslash = wcsrchr(prog_name, '\\');
  if (backslash)
    backslash[1] = 0;
  wcscat_s(prog_name, MAX_PATH, L"debug_message.exe");

  // stupid CreateProcess requires a non-const command line and may modify it.
  // We also want to use the wide string
  int charcount = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
  if (!charcount)
    return;
  scoped_array<wchar_t> cmdline(new wchar_t[charcount]);
  if (!MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, cmdline.get(), charcount))
    return;

  STARTUPINFO startup_info;
  memset(&startup_info, 0, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);

  PROCESS_INFORMATION process_info;
  if (CreateProcessW(prog_name, cmdline.get(), NULL, NULL, false, 0, NULL,
                     NULL, &startup_info, &process_info)) {
    WaitForSingleObject(process_info.hProcess, INFINITE);
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
  } else {
    // debug process broken, let's just do a message box
    MessageBoxW(NULL, cmdline.get(), L"Fatal error",
                MB_OK | MB_ICONHAND | MB_TOPMOST);
  }
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity,
                       int ctr)
    : severity_(severity) {
  Init(file, line);
}

LogMessage::LogMessage(const char* file, int line, const CheckOpString& result)
    : severity_(LOG_FATAL) {
  Init(file, line);
  stream_ << "Check failed: " << (*result.str_);
}

LogMessage::LogMessage(const char* file, int line)
     : severity_(LOG_INFO) {
  Init(file, line);
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : severity_(severity) {
  Init(file, line);
}

// writes the common header info to the stream
void LogMessage::Init(const char* file, int line) {
  // log only the filename
  const char* last_slash = strrchr(file, '\\');
  if (last_slash)
    file = last_slash + 1;

  // TODO(darin): It might be nice if the columns were fixed width.

  stream_ <<  '[';
  if (log_process_id)
    stream_ << GetCurrentProcessId() << ':';
  if (log_thread_id)
    stream_ << GetCurrentThreadId() << ':';
  if (log_timestamp) {
     time_t t = time(NULL);
#if _MSC_VER >= 1400
    struct tm local_time = {0};
    localtime_s(&local_time, &t);
    struct tm* tm_time = &local_time;
#else
    struct tm* tm_time = localtime(&t);
#endif
    stream_ << std::setfill('0')
            << std::setw(2) << 1 + tm_time->tm_mon
            << std::setw(2) << tm_time->tm_mday
            << '/'
            << std::setw(2) << tm_time->tm_hour
            << std::setw(2) << tm_time->tm_min
            << std::setw(2) << tm_time->tm_sec
            << ':';
  }
  if (log_tickcount)
    stream_ << GetTickCount() << ':';
  stream_ << log_severity_names[severity_] << ":" << file << "(" << line << ")] ";

  message_start_ = stream_.tellp();
}

LogMessage::~LogMessage() {
  // TODO(brettw) modify the macros so that nothing is executed when the log
  // level is too high.
  if (severity_ < min_log_level)
    return;

  std::string str_newline(stream_.str());
  str_newline.append("\r\n");

  if (log_filter_prefix && severity_ <= kMaxFilteredLogLevel &&
      str_newline.compare(message_start_, strlen(log_filter_prefix),
                          log_filter_prefix) != 0) {
    return;
  }

  if (logging_destination == LOG_ONLY_TO_SYSTEM_DEBUG_LOG ||
      logging_destination == LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG)
    OutputDebugStringA(str_newline.c_str());

  // write to log file
  if (logging_destination != LOG_NONE &&
      logging_destination != LOG_ONLY_TO_SYSTEM_DEBUG_LOG &&
      InitializeLogFileHandle()) {
    // we can have multiple threads and/or processes, so try to prevent them from
    // clobbering each other's writes
    if (lock_log_file == LOCK_LOG_FILE) {
      // Ensure that the mutex is initialized in case the client app did not
      // call InitLogging. This is not thread safe. See below
      InitLogMutex();

      DWORD r = ::WaitForSingleObject(log_mutex, INFINITE);
      DCHECK(r != WAIT_ABANDONED);
    } else {
      // use the lock
      if (!log_lock) {
        // The client app did not call InitLogging, and so the lock has not
        // been created. We do this on demand, but if two threads try to do
        // this at the same time, there will be a race condition to create
        // the lock. This is why InitLogging should be called from the main
        // thread at the beginning of execution.
        log_lock = new LockImpl();
      }
      log_lock->Lock();
    }

    SetFilePointer(log_file, 0, 0, SEEK_END);
    DWORD num_written;
    WriteFile(log_file, (void*)str_newline.c_str(), (DWORD)str_newline.length(), &num_written, NULL);

    if (lock_log_file == LOCK_LOG_FILE) {
      ReleaseMutex(log_mutex);
    } else {
      log_lock->Unlock();
    }
  }

  if (severity_ == LOG_FATAL) {
    // display a message or break into the debugger on a fatal error
    if (::IsDebuggerPresent()) {
      __debugbreak();
    } else {
      if (log_assert_handler) {
        // make a copy of the string for the handler out of paranoia
        log_assert_handler(std::string(stream_.str()));
      } else {
        // don't use the string with the newline, get a fresh version to send to
        // the debug message process
        DisplayDebugMessage(stream_.str());
        // Crash the process to generate a dump.
        __debugbreak();
      }
    }
  }
}

void CloseLogFile() {
  if (!log_file)
    return;

  CloseHandle(log_file);
  log_file = NULL;
}

} // namespace logging

std::ostream& operator<<(std::ostream& out, const wchar_t* wstr) {
  if (!wstr || !wstr[0])
    return out;

  // compute the length of the buffer we'll need
  int charcount = WideCharToMultiByte(CP_UTF8, 0, wstr, -1,
                                      NULL, 0, NULL, NULL);
  if (charcount == 0)
    return out;

  // convert
  scoped_array<char> buf(new char[charcount]);
  WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buf.get(), charcount, NULL, NULL);
  return out << buf.get();
}
