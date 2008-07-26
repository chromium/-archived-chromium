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

// This interfaces is for managing the global services of the application. Each
// service is lazily created when requested the first time. The service getters
// will return NULL if the service is not available, so callers must check for
// this condition.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_H__
#define CHROME_BROWSER_BROWSER_PROCESS_H__

#include <string>

#include "base/basictypes.h"
#include "base/message_loop.h"

class AutomationProviderList;
class ClipboardService;
class GoogleURLTracker;
class IconManager;
class MetricsService;
class NotificationService;
class PrefService;
class ProfileManager;
class RenderProcessHost;
class ResourceDispatcherHost;
class DebuggerWrapper;
class Thread;
class WebAppInstallerService;
class SuspendController;

namespace sandbox {
class BrokerServices;
}
namespace ChromeViews {
class AcceleratorHandler;
}
namespace printing {
class PrintJobManager;
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
  virtual Thread* io_thread() = 0;

  // Returns the thread that we perform random file operations on. For code
  // that wants to do I/O operations (not network requests or even file: URL
  // requests), this is the thread to use to avoid blocking the UI thread.
  // It might be nicer to have a thread pool for this kind of thing.
  virtual Thread* file_thread() = 0;

  // Returns the thread that is used for database operations such as history.
  virtual Thread* db_thread() = 0;

  virtual sandbox::BrokerServices* broker_services() = 0;

  virtual IconManager* icon_manager() = 0;

  virtual void InitBrokerServices(sandbox::BrokerServices*) = 0;
  virtual AutomationProviderList* InitAutomationProviderList() = 0;

  virtual void InitDebuggerWrapper(int port) = 0;

  virtual unsigned int AddRefModule() = 0;
  virtual unsigned int ReleaseModule() = 0;

  virtual bool IsShuttingDown() = 0;

  virtual ChromeViews::AcceleratorHandler* accelerator_handler() = 0;

  virtual printing::PrintJobManager* print_job_manager() = 0;

  virtual GoogleURLTracker* google_url_tracker() = 0;

  // Returns the locale used by the application.
  virtual const std::wstring& GetApplicationLocale() = 0;

  virtual MemoryModel memory_model() = 0;

  virtual SuspendController* suspend_controller() = 0;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(BrowserProcess);
};

extern BrowserProcess* g_browser_process;

#endif  // CHROME_BROWSER_BROWSER_PROCESS_H__
