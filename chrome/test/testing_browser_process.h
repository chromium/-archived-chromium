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

// An implementation of BrowserProcess for unit tests that fails for most
// services. By preventing creation of services, we reduce dependencies and
// keep the profile clean. Clients of this class must handle the NULL return
// value, however.

#ifndef CHROME_TEST_TESTING_BROWSER_PROCESS_H__
#define CHROME_TEST_TESTING_BROWSER_PROCESS_H__

#include <string>

#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/notification_service.h"
#include "base/logging.h"

class TestingBrowserProcess : public BrowserProcess {
 public:
  TestingBrowserProcess() {}
  virtual ~TestingBrowserProcess() {
  }

  virtual void EndSession() {
  }

  virtual ResourceDispatcherHost* resource_dispatcher_host() {
    return NULL;
  }

  virtual MetricsService* metrics_service() {
    return NULL;
  }

  virtual Thread* io_thread() {
    return NULL;
  }

  virtual Thread* file_thread() {
    return NULL;
  }

  virtual Thread* db_thread() {
    return NULL;
  }

  virtual ProfileManager* profile_manager() {
    return NULL;
  }

  virtual PrefService* local_state() {
    return NULL;
  }

  virtual WebAppInstallerService* web_app_installer_service() {
    return NULL;
  }

  virtual IconManager* icon_manager() {
    return NULL;
  }

  virtual sandbox::BrokerServices* broker_services() {
    return NULL;
  }

  virtual DebuggerWrapper* debugger_wrapper() {
    return NULL;
  }

  virtual ClipboardService* clipboard_service() {
    return NULL;
  }

  virtual GoogleURLTracker* google_url_tracker() {
    return NULL;
  }

  virtual void InitBrokerServices(sandbox::BrokerServices*) {
  }

  virtual AutomationProviderList* InitAutomationProviderList() {
    return NULL;
  }

  virtual void InitDebuggerWrapper(int port) {
  }

  virtual unsigned int AddRefModule() {
    return 1;
  }
  virtual unsigned int ReleaseModule() {
    return 1;
  }

  virtual bool IsShuttingDown() {
    return false;
  }

  virtual ChromeViews::AcceleratorHandler* accelerator_handler() {
    return NULL;
  }

  virtual printing::PrintJobManager* print_job_manager() {
    return NULL;
  }

  virtual const std::wstring& GetApplicationLocale() {
    static std::wstring* value = NULL;
    if (!value)
      value = new std::wstring(L"en");
    return *value;
  }

  virtual MemoryModel memory_model() { return HIGH_MEMORY_MODEL; }

  virtual SuspendController* suspend_controller() { return NULL; }

  virtual bool IsUsingNewFrames() { return false; }

 private:
  NotificationService notification_service_;
  DISALLOW_EVIL_CONSTRUCTORS(TestingBrowserProcess);
};

#endif  // CHROME_TEST_TESTING_BROWSER_PROCESS_H__
