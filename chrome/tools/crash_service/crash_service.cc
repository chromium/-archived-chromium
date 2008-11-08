// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/crash_service/crash_service.h"

#include <windows.h>

#include <iostream>
#include <fstream>
#include <map>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "breakpad/src/client/windows/crash_generation/crash_generation_server.h"
#include "breakpad/src/client/windows/sender/crash_report_sender.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/win_util.h"

// TODO(cpu): Bug 1169078. There is a laundry list of things to do for this
// application. They will be addressed as they are required.

namespace {

const wchar_t kTestPipeName[] = L"\\\\.\\pipe\\ChromeCrashServices";

const wchar_t kCrashReportURL[] = L"https://clients2.google.com/cr/report";
const wchar_t kCheckPointFile[] = L"crash_checkpoint.txt";

typedef std::map<std::wstring, std::wstring> CrashMap;

bool CustomInfoToMap(const google_breakpad::ClientInfo* client_info,
                     const std::wstring& reporter_tag, CrashMap* map) {
  google_breakpad::CustomClientInfo info = client_info->GetCustomInfo();

  for (int i = 0; i < info.count; ++i) {
    (*map)[info.entries[i].name] = info.entries[i].value;
  }

  (*map)[L"rept"] = reporter_tag;

  return (map->size() > 0);
}

bool WriteCustomInfoToFile(const std::wstring& dump_path, const CrashMap& map) {
  std::wstring file_path(dump_path);
  size_t last_dot = file_path.rfind(L'.');
  if (last_dot == std::wstring::npos)
    return false;
  file_path.resize(last_dot);
  file_path += L".txt";

  std::wofstream file(file_path.c_str(),
      std::ios_base::out | std::ios_base::app | std::ios::binary);
  if (!file.is_open())
    return false;

  CrashMap::const_iterator pos;
  for (pos = map.begin(); pos != map.end(); ++pos) {
    std::wstring line = pos->first;
    line += L':';
    line += pos->second;
    line += L'\n';
    file.write(line.c_str(), static_cast<std::streamsize>(line.length()));
  }
  return true;
}

// The window procedure task is to handle when a) the user logs off.
// b) the system shuts down or c) when the user closes the window.
LRESULT __stdcall CrashSvcWndProc(HWND hwnd, UINT message,
                                  WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_CLOSE:
    case WM_ENDSESSION:
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      return DefWindowProc(hwnd, message, wparam, lparam);
  }
  return 0;
}

// This is the main and only application window.
HWND g_top_window = NULL;

bool CreateTopWindow(HINSTANCE instance, bool visible) {
  WNDCLASSEXW wcx = {0};
  wcx.cbSize = sizeof(wcx);
  wcx.style = CS_HREDRAW | CS_VREDRAW;
  wcx.lpfnWndProc = CrashSvcWndProc;
  wcx.hInstance = instance;
  wcx.lpszClassName = L"crash_svc_class";
  ATOM atom = ::RegisterClassExW(&wcx);
  DWORD style = visible ? WS_POPUPWINDOW | WS_VISIBLE : WS_OVERLAPPED;

  // The window size is zero but being a popup window still shows in the
  // task bar and can be closed using the system menu or using task manager.
  HWND window = CreateWindowExW(0, wcx.lpszClassName, L"crash service", style,
                                CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
                                NULL, NULL, instance, NULL);
  if (!window)
    return false;

  ::UpdateWindow(window);
  LOG(INFO) << "window handle is " << window;
  g_top_window = window;
  return true;
}

// Simple helper class to keep the process alive until the current request
// finishes.
class ProcessingLock {
 public:
  ProcessingLock() {
    ::InterlockedIncrement(&op_count_);
  }
  ~ProcessingLock() {
    ::InterlockedDecrement(&op_count_);
  }
  static bool IsWorking() {
    return (op_count_ != 0);
  }
 private:
  static volatile LONG op_count_;
};

volatile LONG ProcessingLock::op_count_ = 0;

// This structure contains the information that the worker thread needs to
// send a crash dump to the server.
struct DumpJobInfo {
  DWORD pid;
  CrashService* self;
  CrashMap map;
  std::wstring dump_path;

  DumpJobInfo(DWORD process_id, CrashService* service,
              const CrashMap& crash_map, const std::wstring& path)
      : pid(process_id), self(service), map(crash_map), dump_path(path) {
  }
};

}  // namespace

// Command line switches:
const wchar_t CrashService::kMaxReports[] = L"max-reports";
const wchar_t CrashService::kNoWindow[] = L"no-window";
const wchar_t CrashService::kReporterTag[]= L"reporter";

CrashService::CrashService(const std::wstring& report_dir)
    : report_path_(report_dir),
      sender_(NULL),
      dumper_(NULL),
      requests_handled_(0),
      requests_sent_(0),
      clients_connected_(0),
      clients_terminated_(0) {
  chrome::RegisterPathProvider();
}

CrashService::~CrashService() {
  AutoLock lock(sending_);
  delete dumper_;
  delete sender_;
}

bool CrashService::Initialize(const std::wstring& command_line) {
  using google_breakpad::CrashReportSender;
  using google_breakpad::CrashGenerationServer;

  const wchar_t* pipe_name = kTestPipeName;
  std::wstring dumps_path;
  int max_reports = -1;

  // The checkpoint file allows CrashReportSender to enforce the the maximum
  // reports per day quota. Does not seem to serve any other purpose.
  std::wstring checkpoint_path = report_path_;
  file_util::AppendToPath(&checkpoint_path, kCheckPointFile);

  // The dumps path is typically : '<user profile>\Local settings\
  // Application data\Goggle\Chrome\Crash Reports' and the report path is
  // Application data\Google\Chrome\Reported Crashes.txt
  if (!PathService::Get(chrome::DIR_USER_DATA, &report_path_)) {
    LOG(ERROR) << "could not get DIR_USER_DATA";
    return false;
  }
  file_util::AppendToPath(&report_path_, chrome::kCrashReportLog);
  if (!PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path)) {
    LOG(ERROR) << "could not get DIR_CRASH_DUMPS";
    return false;
  }

  CommandLine cmd_line(command_line);

  // We can override the send reports quota with a command line switch.
  if (cmd_line.HasSwitch(kMaxReports))
    max_reports = _wtoi(cmd_line.GetSwitchValue(kMaxReports).c_str());

  if (max_reports > 0) {
    // Create the http sender object.
    sender_ = new CrashReportSender(checkpoint_path);
    if (!sender_) {
      LOG(ERROR) << "could not create sender";
      return false;
    }
    sender_->set_max_reports_per_day(max_reports);
  }
  // Create the OOP crash generator object.
  dumper_ = new CrashGenerationServer(pipe_name, NULL,
                                      &CrashService::OnClientConnected, this,
                                      &CrashService::OnClientDumpRequest, this,
                                      &CrashService::OnClientExited, this,
                                      true, &dumps_path);
  if (!dumper_) {
    LOG(ERROR) << "could not create dumper";
    return false;
  }

  if (!CreateTopWindow(::GetModuleHandleW(NULL),
                       !cmd_line.HasSwitch(kNoWindow))) {
    LOG(ERROR) << "could not create window";
    return false;
  }

  reporter_tag_ = L"crash svc";
  if (cmd_line.HasSwitch(kReporterTag))
    reporter_tag_ = cmd_line.GetSwitchValue(kReporterTag);

  // Log basic information.
  LOG(INFO) << "pipe name is " << pipe_name;
  LOG(INFO) << "dumps at " << dumps_path;
  LOG(INFO) << "reports at " << report_path_;

  if (sender_) {
    LOG(INFO) << "checkpoint is " << checkpoint_path;
    LOG(INFO) << "server is " << kCrashReportURL;
    LOG(INFO) << "maximum " << sender_->max_reports_per_day() << " reports/day";
    LOG(INFO) << "reporter is " << reporter_tag_;
  }
  // Start servicing clients.
  if (!dumper_->Start()) {
    LOG(ERROR) << "could not start dumper";
    return false;
  }

  // This is throwaway code. We don't need to sync with the browser process
  // once Google Update is updated to a version supporting OOP crash handling.
  // Create or open an event to signal the browser process that the crash
  // service is initialized.
  HANDLE running_event =
      ::CreateEventW(NULL, TRUE, TRUE, L"g_chrome_crash_svc");
  // If the browser already had the event open, the CreateEvent call did not
  // signal it. We need to do it manually.
  ::SetEvent(running_event);

  return true;
}

void CrashService::OnClientConnected(void* context,
    const google_breakpad::ClientInfo* client_info) {
  ProcessingLock lock;
  LOG(INFO) << "client start. pid = " << client_info->pid();
  CrashService* self = static_cast<CrashService*>(context);
  ::InterlockedIncrement(&self->clients_connected_);
}

void CrashService::OnClientExited(void* context,
    const google_breakpad::ClientInfo* client_info) {
  ProcessingLock lock;
  LOG(INFO) << "client end. pid = " << client_info->pid();
  CrashService* self = static_cast<CrashService*>(context);
  ::InterlockedIncrement(&self->clients_terminated_);

  if (!self->sender_)
    return;

  // When we are instructed to send reports we need to exit if there are
  // no more clients to service. The next client that runs will start us.
  // Only chrome.exe starts crash_service with a non-zero max_reports.
  if (self->clients_connected_ > self->clients_terminated_)
    return;
  if (self->sender_->max_reports_per_day() > 0) {
    // Wait for the other thread to send crashes, if applicable. The sender
    // thread takes the sending_ lock, so the sleep is just to give it a
    // chance to start.
    ::Sleep(1000);
    AutoLock lock(self->sending_);
    // Some people can restart chrome very fast, check again if we have
    // a new client before exiting for real.
    if (self->clients_connected_ == self->clients_terminated_) {
      LOG(INFO) << "zero clients. exiting";
      ::PostMessage(g_top_window, WM_CLOSE, 0, 0);
    }
  }
}

void CrashService::OnClientDumpRequest(void* context,
    const google_breakpad::ClientInfo* client_info,
    const std::wstring* file_path) {
  ProcessingLock lock;

  if (!file_path) {
    LOG(ERROR) << "dump with no file path";
    return;
  }
  if (!client_info) {
    LOG(ERROR) << "dump with no client info";
    return;
  }

  DWORD pid = client_info->pid();
  LOG(INFO) << "dump for pid = " << pid << " is " << *file_path;

  CrashService* self = static_cast<CrashService*>(context);
  if (!self) {
    LOG(ERROR) << "dump with no context";
    return;
  }

  CrashMap map;
  CustomInfoToMap(client_info, self->reporter_tag_, &map);

  if (!WriteCustomInfoToFile(*file_path, map)) {
    LOG(ERROR) << "could not write custom info file";
  }

  if (!self->sender_)
    return;

  // Send the crash dump using a worker thread. This operation has retry
  // logic in case there is no internet connection at the time.
  DumpJobInfo* dump_job = new DumpJobInfo(pid, self, map, *file_path);
  if (!::QueueUserWorkItem(&CrashService::AsyncSendDump,
                           dump_job, WT_EXECUTELONGFUNCTION)) {
    LOG(ERROR) << "could not queue job";
  }
}

// We are going to try sending the report several times. If we can't send,
// we sleep from one minute to several hours depending on the retry round.
unsigned long CrashService::AsyncSendDump(void* context) {
  if (!context)
    return 0;

  DumpJobInfo* info = static_cast<DumpJobInfo*>(context);

  std::wstring report_id = L"<unsent>";

  const DWORD kOneMinute = 60*1000;
  const DWORD kOneHour = 60*kOneMinute;

  const DWORD kSleepSchedule[] = {
      24*kOneHour,
      8*kOneHour,
      4*kOneHour,
      kOneHour,
      15*kOneMinute,
      0};

  int retry_round = arraysize(kSleepSchedule) - 1;

  do {
    ::Sleep(kSleepSchedule[retry_round]);
    {
      // Take the server lock while sending. This also prevent early
      // termination of the service object.
      AutoLock lock(info->self->sending_);
      LOG(INFO) << "trying to send report for pid = " << info->pid;
      google_breakpad::ReportResult send_result
          = info->self->sender_->SendCrashReport(kCrashReportURL, info->map,
                                                 info->dump_path, &report_id);
      switch (send_result) {
        case google_breakpad::RESULT_FAILED:
          report_id = L"<network issue>";
          break;
        case google_breakpad::RESULT_REJECTED:
          report_id = L"<rejected>";
          ++info->self->requests_handled_;
          retry_round = 0;
          break;
        case google_breakpad::RESULT_SUCCEEDED:
          ++info->self->requests_sent_;
          ++info->self->requests_handled_;
          retry_round = 0;
          break;
        case google_breakpad::RESULT_THROTTLED:
          report_id = L"<throttled>";
          break;
        default:
          report_id = L"<unknown>";
          break;
      };
    }

    LOG(INFO) << "dump for pid =" << info->pid << " crash2 id =" << report_id;
    --retry_round;
  } while(retry_round >= 0);

  if (!::DeleteFileW(info->dump_path.c_str()))
    LOG(WARNING) << "could not delete " << info->dump_path;

  delete info;
  return 0;
}

int CrashService::ProcessingLoop() {
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  LOG(INFO) << "session ending..";
  while (ProcessingLock::IsWorking()) {
    ::Sleep(50);
  }

  LOG(INFO) << "clients connected :" << clients_connected_;
  LOG(INFO) << "clients terminated :" << clients_terminated_;
  LOG(INFO) << "dumps serviced :" << requests_handled_;
  LOG(INFO) << "dumps reported :" << requests_sent_;

  return static_cast<int>(msg.wParam);
}

