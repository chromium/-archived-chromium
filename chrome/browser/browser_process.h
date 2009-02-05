// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This interfaces is for managing the global services of the application. Each
// service is lazily created when requested the first time. The service getters
// will return NULL if the service is not available, so callers must check for
// this condition.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_H_
#define CHROME_BROWSER_BROWSER_PROCESS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

class AutomationProviderList;
class ClipboardService;
class DownloadRequestManager;
class GoogleURLTracker;
class IconManager;
class MetricsService;
class PrefService;
class ProfileManager;
class DebuggerWrapper;
class ResourceDispatcherHost;
class WebAppInstallerService;
class SuspendController;

namespace base {
class Thread;
class WaitableEvent;
}
namespace sandbox {
class BrokerServices;
}
namespace printing {
class PrintJobManager;
}
namespace views {
class AcceleratorHandler;
}

// NOT THREAD SAFE, call only from the main thread.
// These functions shouldn't return NULL unless otherwise noted.
class BrowserProcess {
 public:
  BrowserProcess() {}
  virtual ~BrowserProcess() {}

  // The browser has 3 memory model configurations.  These models have to
  // do with how aggressively we release Renderer memory to the OS.
  // Low memory releases memory the fastest, High memory releases it the
  // slowest.  Geek out!
  enum MemoryModel {
    // Will release as much memory as it can after each tab switch, and also
    // after user idle.
    LOW_MEMORY_MODEL,
    // Will release a little memory after each tab switch and also after
    // user idle.
    MEDIUM_MEMORY_MODEL,
    // Hangs onto every last byte.
    HIGH_MEMORY_MODEL
  };

  // Invoked when the user is logging out/shutting down. When logging off we may
  // not have enough time to do a normal shutdown. This method is invoked prior
  // to normal shutdown and saves any state that must be saved before we are
  // continue shutdown.
  virtual void EndSession() = 0;

  // Services: any of these getters may return NULL
  virtual ResourceDispatcherHost* resource_dispatcher_host() = 0;

  virtual MetricsService* metrics_service() = 0;
  virtual ProfileManager* profile_manager() = 0;
  virtual PrefService* local_state() = 0;
  virtual DebuggerWrapper* debugger_wrapper() = 0;
  virtual ClipboardService* clipboard_service() = 0;

  // Returns the thread that we perform I/O coordination on (network requests,
  // communication with renderers, etc.
  // NOTE: need to check the return value for NULL.
  virtual base::Thread* io_thread() = 0;

  // Returns the thread that we perform random file operations on. For code
  // that wants to do I/O operations (not network requests or even file: URL
  // requests), this is the thread to use to avoid blocking the UI thread.
  // It might be nicer to have a thread pool for this kind of thing.
  virtual base::Thread* file_thread() = 0;

  // Returns the thread that is used for database operations such as the web
  // database. History has its own thread since it has much higher traffic.
  virtual base::Thread* db_thread() = 0;

  virtual sandbox::BrokerServices* broker_services() = 0;

  virtual IconManager* icon_manager() = 0;

  virtual void InitBrokerServices(sandbox::BrokerServices*) = 0;
  virtual AutomationProviderList* InitAutomationProviderList() = 0;

  virtual void InitDebuggerWrapper(int port) = 0;

  virtual unsigned int AddRefModule() = 0;
  virtual unsigned int ReleaseModule() = 0;

  virtual bool IsShuttingDown() = 0;

  virtual views::AcceleratorHandler* accelerator_handler() = 0;

  virtual printing::PrintJobManager* print_job_manager() = 0;

  virtual GoogleURLTracker* google_url_tracker() = 0;

  // Returns the locale used by the application.
  virtual const std::wstring& GetApplicationLocale() = 0;

  virtual MemoryModel memory_model() = 0;

#if defined(OS_WIN)
  DownloadRequestManager* download_request_manager();
#endif

  // Returns an event that is signaled when the browser shutdown.
  virtual base::WaitableEvent* shutdown_event() = 0;

  // Returns a reference to the user-data-dir based profiles vector.
  std::vector<std::wstring>& user_data_dir_profiles() {
    return user_data_dir_profiles_;
  }

 private:
  // User-data-dir based profiles.
  std::vector<std::wstring> user_data_dir_profiles_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcess);
};

extern BrowserProcess* g_browser_process;

#endif  // CHROME_BROWSER_BROWSER_PROCESS_H_
