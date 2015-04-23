// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// When each service is created, we set a flag indicating this. At this point,
// the service initialization could fail or succeed. This allows us to remember
// if we tried to create a service, and not try creating it over and over if
// the creation failed.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_
#define CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/non_thread_safe.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"

#if defined(OS_WIN)
#include "sandbox/src/sandbox.h"
#else
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

class CommandLine;
class NotificationService;

// Real implementation of BrowserProcess that creates and returns the services.
class BrowserProcessImpl : public BrowserProcess, public NonThreadSafe {
 public:
  BrowserProcessImpl(const CommandLine& command_line);
  virtual ~BrowserProcessImpl();

  virtual void EndSession();

  virtual ResourceDispatcherHost* resource_dispatcher_host() {
    DCHECK(CalledOnValidThread());
    if (!created_resource_dispatcher_host_)
      CreateResourceDispatcherHost();
    return resource_dispatcher_host_.get();
  }

  virtual MetricsService* metrics_service() {
    DCHECK(CalledOnValidThread());
    if (!created_metrics_service_)
      CreateMetricsService();
    return metrics_service_.get();
  }

  virtual base::Thread* io_thread() {
    DCHECK(CalledOnValidThread());
    if (!created_io_thread_)
      CreateIOThread();
    return io_thread_.get();
  }

  virtual base::Thread* file_thread() {
    DCHECK(CalledOnValidThread());
    if (!created_file_thread_)
      CreateFileThread();
    return file_thread_.get();
  }

  virtual base::Thread* db_thread() {
    DCHECK(CalledOnValidThread());
    if (!created_db_thread_)
      CreateDBThread();
    return db_thread_.get();
  }

#if defined(OS_LINUX)
  virtual base::Thread* background_x11_thread() {
    DCHECK(CalledOnValidThread());
    // The BACKGROUND_X11 thread is created when the IO thread is created.
    if (!created_io_thread_)
      CreateIOThread();
    return background_x11_thread_.get();
  }
#endif

  virtual ProfileManager* profile_manager() {
    DCHECK(CalledOnValidThread());
    if (!created_profile_manager_)
      CreateProfileManager();
    return profile_manager_.get();
  }

  virtual PrefService* local_state() {
    DCHECK(CalledOnValidThread());
    if (!created_local_state_)
      CreateLocalState();
    return local_state_.get();
  }

  virtual sandbox::BrokerServices* broker_services() {
    // TODO(abarth): DCHECK(CalledOnValidThread());
    //               See <http://b/1287166>.
    if (!initialized_broker_services_)
      return NULL;
    return broker_services_;
  }

  virtual DebuggerWrapper* debugger_wrapper() {
    DCHECK(CalledOnValidThread());
    if (!created_debugger_wrapper_)
      return NULL;
    return debugger_wrapper_.get();
  }

  virtual DevToolsManager* devtools_manager() {
    DCHECK(CalledOnValidThread());
    if (!created_devtools_manager_)
      CreateDevToolsManager();
    return devtools_manager_.get();
  }

  virtual Clipboard* clipboard() {
    DCHECK(CalledOnValidThread());
    return clipboard_.get();
  }

  virtual IconManager* icon_manager() {
    DCHECK(CalledOnValidThread());
    if (!created_icon_manager_)
      CreateIconManager();
    return icon_manager_.get();
  }

  virtual ThumbnailGenerator* GetThumbnailGenerator() {
    return &thumbnail_generator_;
  }

  virtual AutomationProviderList* InitAutomationProviderList() {
    DCHECK(CalledOnValidThread());
    if (automation_provider_list_.get() == NULL) {
      automation_provider_list_.reset(AutomationProviderList::GetInstance());
    }
    return automation_provider_list_.get();
  }

  virtual void InitDebuggerWrapper(int port) {
    DCHECK(CalledOnValidThread());
    if (!created_debugger_wrapper_)
      CreateDebuggerWrapper(port);
  }

  virtual unsigned int AddRefModule() {
    DCHECK(CalledOnValidThread());
    module_ref_count_++;
    return module_ref_count_;
  }

  virtual unsigned int ReleaseModule() {
    DCHECK(CalledOnValidThread());
    DCHECK(0 != module_ref_count_);
    module_ref_count_--;
    if (0 == module_ref_count_) {
      MessageLoop::current()->Quit();
    }
    return module_ref_count_;
  }

  virtual bool IsShuttingDown() {
    DCHECK(CalledOnValidThread());
    return 0 == module_ref_count_;
  }

  virtual views::AcceleratorHandler* accelerator_handler() {
    DCHECK(CalledOnValidThread());
    if (!accelerator_handler_.get())
      CreateAcceleratorHandler();
    return accelerator_handler_.get();
  }

  virtual printing::PrintJobManager* print_job_manager();

  virtual GoogleURLTracker* google_url_tracker() {
    DCHECK(CalledOnValidThread());
    if (!google_url_tracker_.get())
      CreateGoogleURLTracker();
    return google_url_tracker_.get();
  }

  virtual const std::string& GetApplicationLocale();

  virtual MemoryModel memory_model() {
    DCHECK(CalledOnValidThread());
    return memory_model_;
  }

  virtual base::WaitableEvent* shutdown_event() {
    return shutdown_event_.get();
  }

 private:
  void CreateResourceDispatcherHost();
  void CreatePrefService();
  void CreateMetricsService();
  void CreateIOThread();
  void CreateFileThread();
  void CreateDBThread();
  void CreateTemplateURLModel();
  void CreateProfileManager();
  void CreateWebDataService();
  void CreateLocalState();
  void CreateViewedPageTracker();
  void CreateIconManager();
  void CreateDebuggerWrapper(int port);
  void CreateDevToolsManager();
  void CreateAcceleratorHandler();
  void CreateGoogleURLTracker();

  void InitBrokerServices(sandbox::BrokerServices* broker_services);

  bool created_resource_dispatcher_host_;
  scoped_ptr<ResourceDispatcherHost> resource_dispatcher_host_;

  bool created_metrics_service_;
  scoped_ptr<MetricsService> metrics_service_;

  bool created_io_thread_;
  scoped_ptr<base::Thread> io_thread_;
#if defined(OS_LINUX)
  // This shares a created flag with the IO thread.
  scoped_ptr<base::Thread> background_x11_thread_;
#endif

  bool created_file_thread_;
  scoped_ptr<base::Thread> file_thread_;

  bool created_db_thread_;
  scoped_ptr<base::Thread> db_thread_;

  bool created_profile_manager_;
  scoped_ptr<ProfileManager> profile_manager_;

  bool created_local_state_;
  scoped_ptr<PrefService> local_state_;

  bool initialized_broker_services_;
  sandbox::BrokerServices* broker_services_;

  bool created_icon_manager_;
  scoped_ptr<IconManager> icon_manager_;

  bool created_debugger_wrapper_;
  scoped_refptr<DebuggerWrapper> debugger_wrapper_;

  bool created_devtools_manager_;
  scoped_refptr<DevToolsManager> devtools_manager_;

  scoped_ptr<Clipboard> clipboard_;

  scoped_ptr<AutomationProviderList> automation_provider_list_;

  scoped_ptr<views::AcceleratorHandler> accelerator_handler_;

  scoped_ptr<GoogleURLTracker> google_url_tracker_;

  scoped_ptr<NotificationService> main_notification_service_;

  unsigned int module_ref_count_;

  // Ensures that all the print jobs are finished before closing the browser.
  scoped_ptr<printing::PrintJobManager> print_job_manager_;

  std::string locale_;

  MemoryModel memory_model_;

  bool checked_for_new_frames_;
  bool using_new_frames_;

  // This service just sits around and makes thumanails for tabs. It does
  // nothing in the cosntructor so we don't have to worry about lazy init.
  ThumbnailGenerator thumbnail_generator_;

  // An event that notifies when we are shutting-down.
  scoped_ptr<base::WaitableEvent> shutdown_event_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessImpl);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_
