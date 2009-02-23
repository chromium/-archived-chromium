// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_impl.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "chrome/browser/browser_trial.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/debugger/debugger_wrapper.h"
#include "chrome/browser/download/download_file.h"
#include "chrome/browser/download/save_file_manager.h"
#include "chrome/browser/google_url_tracker.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

#if defined(OS_WIN)
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/common/clipboard_service.h"
#include "chrome/views/accelerator_handler.h"
#include "chrome/views/view_storage.h"
#elif defined(OS_POSIX)
// TODO(port): Remove the temporary scaffolding as we port the above headers.
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

namespace {

// ----------------------------------------------------------------------------
// BrowserProcessSubThread
//
// This simple thread object is used for the specialized threads that the
// BrowserProcess spins up.
//
// Applications must initialize the COM library before they can call
// COM library functions other than CoGetMalloc and memory allocation
// functions, so this class initializes COM for those users.
class BrowserProcessSubThread : public ChromeThread {
 public:
  explicit BrowserProcessSubThread(ChromeThread::ID identifier)
      : ChromeThread(identifier) {
  }

  ~BrowserProcessSubThread() {
    // We cannot rely on our base class to stop the thread since we want our
    // CleanUp function to run.
    Stop();
  }

 protected:
  virtual void Init() {
#if defined(OS_WIN)
    // Initializes the COM library on the current thread.
    CoInitialize(NULL);
#endif

    notification_service_ = new NotificationService;
  }

  virtual void CleanUp() {
    delete notification_service_;
    notification_service_ = NULL;

#if defined(OS_WIN)
    // Closes the COM library on the current thread. CoInitialize must
    // be balanced by a corresponding call to CoUninitialize.
    CoUninitialize();
#endif
  }

 private:
  // Each specialized thread has its own notification service.
  // Note: We don't use scoped_ptr because the destructor runs on the wrong
  // thread.
  NotificationService* notification_service_;
};

}  // namespace

BrowserProcessImpl::BrowserProcessImpl(const CommandLine& command_line)
    : created_resource_dispatcher_host_(false),
      created_metrics_service_(false),
      created_io_thread_(false),
      created_file_thread_(false),
      created_db_thread_(false),
      created_profile_manager_(false),
      created_local_state_(false),
      initialized_broker_services_(false),
      broker_services_(NULL),
      created_icon_manager_(false),
      created_debugger_wrapper_(false),
      module_ref_count_(0),
      memory_model_(MEDIUM_MEMORY_MODEL),
      checked_for_new_frames_(false),
      using_new_frames_(false) {
  g_browser_process = this;
  clipboard_service_.reset(new ClipboardService);
  main_notification_service_.reset(new NotificationService);

  // Must be created after the NotificationService.
  print_job_manager_.reset(new printing::PrintJobManager);

  // Configure the browser memory model.
  if (command_line.HasSwitch(switches::kMemoryModel)) {
    std::wstring model = command_line.GetSwitchValue(switches::kMemoryModel);
    if (!model.empty()) {
      if (model == L"high")
        memory_model_ = HIGH_MEMORY_MODEL;
      else if (model == L"low")
        memory_model_ = LOW_MEMORY_MODEL;
      else if (model == L"medium")
        memory_model_ = MEDIUM_MEMORY_MODEL;
    }
  } else {
    // Randomly choose what memory model to use.
    const double probability = 0.5;
    FieldTrial* trial(new FieldTrial(BrowserTrial::kMemoryModelFieldTrial,
                                     probability));
    DCHECK(FieldTrialList::Find(BrowserTrial::kMemoryModelFieldTrial) == trial);
    if (trial->boolean_value())
      memory_model_ = HIGH_MEMORY_MODEL;
    else
      memory_model_ = MEDIUM_MEMORY_MODEL;
  }

  shutdown_event_.reset(new base::WaitableEvent(true, false));
}

BrowserProcessImpl::~BrowserProcessImpl() {
  // Delete the AutomationProviderList before NotificationService,
  // since it may try to unregister notifications
  // Both NotificationService and AutomationProvider are singleton instances in
  // the BrowserProcess. Since AutomationProvider may have some active
  // notification observers, it is essential that it gets destroyed before the
  // NotificationService. NotificationService won't be destroyed until after
  // this destructor is run.
  automation_provider_list_.reset();

  // We need to destroy the MetricsService and GoogleURLTracker before the
  // io_thread_ gets destroyed, since both destructors can call the URLFetcher
  // destructor, which does an InvokeLater operation on the IO thread.  (The IO
  // thread will handle that URLFetcher operation before going away.)
  metrics_service_.reset();
  google_url_tracker_.reset();

  // Need to clear profiles (download managers) before the io_thread_.
  profile_manager_.reset();

  // Debugger must be cleaned up before IO thread and NotificationService.
  debugger_wrapper_ = NULL;

  if (resource_dispatcher_host_.get()) {
    // Need to tell Safe Browsing Service that the IO thread is going away
    // since it cached a pointer to it.
    if (resource_dispatcher_host()->safe_browsing_service())
      resource_dispatcher_host()->safe_browsing_service()->ShutDown();

    // Cancel pending requests and prevent new requests.
    resource_dispatcher_host()->Shutdown();
  }

  // Shutdown DNS prefetching now to ensure that network stack objects
  // living on the IO thread get destroyed before the IO thread goes away.
  io_thread_->message_loop()->PostTask(FROM_HERE,
      NewRunnableFunction(chrome_browser_net::EnsureDnsPrefetchShutdown));

  // Need to stop io_thread_ before resource_dispatcher_host_, since
  // io_thread_ may still deref ResourceDispatcherHost and handle resource
  // request before going away.
  io_thread_.reset();

  // Clean up state that lives on the file_thread_ before it goes away.
  if (resource_dispatcher_host_.get()) {
    resource_dispatcher_host()->download_file_manager()->Shutdown();
    resource_dispatcher_host()->save_file_manager()->Shutdown();
  }

  // Need to stop the file_thread_ here to force it to process messages in its
  // message loop from the previous call to shutdown the DownloadFileManager,
  // SaveFileManager and SessionService.
  file_thread_.reset();

  // With the file_thread_ flushed, we can release any icon resources.
  icon_manager_.reset();

  // Need to destroy ResourceDispatcherHost before PluginService and
  // SafeBrowsingService, since it caches a pointer to it.
  resource_dispatcher_host_.reset();

  // Wait for the pending print jobs to finish.
  print_job_manager_->OnQuit();
  print_job_manager_.reset();

  // TODO(port): remove this completely from BrowserProcessImpl, it has no
  // business being here.
#if defined(OS_WIN)
  // The ViewStorage needs to go before the NotificationService.
  views::ViewStorage::DeleteSharedInstance();
#endif

  // Now OK to destroy NotificationService.
  main_notification_service_.reset();

  g_browser_process = NULL;
}

// Send a QuitTask to the given MessageLoop.
static void PostQuit(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

void BrowserProcessImpl::EndSession() {
#if defined(OS_WIN)
  // Notify we are going away.
  ::SetEvent(shutdown_event_->handle());
#endif

  // Mark all the profiles as clean.
  ProfileManager* pm = profile_manager();
  for (ProfileManager::const_iterator i = pm->begin(); i != pm->end(); ++i)
    (*i)->MarkAsCleanShutdown();

  // Tell the metrics service it was cleanly shutdown.
  MetricsService* metrics = g_browser_process->metrics_service();
  if (metrics && local_state()) {
    metrics->RecordCleanShutdown();

    metrics->RecordStartOfSessionEnd();

    // MetricsService lazily writes to prefs, force it to write now.
    local_state()->SavePersistentPrefs(file_thread());
  }

  // We must write that the profile and metrics service shutdown cleanly,
  // otherwise on startup we'll think we crashed. So we block until done and
  // then proceed with normal shutdown.
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableFunction(PostQuit, MessageLoop::current()));
  MessageLoop::current()->Run();
}

printing::PrintJobManager* BrowserProcessImpl::print_job_manager() {
  // TODO(abarth): DCHECK(CalledOnValidThread());
  //               See <http://b/1287209>.
  // print_job_manager_ is initialized in the constructor and destroyed in the
  // destructor, so it should always be valid.
  DCHECK(print_job_manager_.get());
  return print_job_manager_.get();
}

const std::wstring& BrowserProcessImpl::GetApplicationLocale() {
  DCHECK(CalledOnValidThread());
  if (locale_.empty()) {
    locale_ = l10n_util::GetApplicationLocale(local_state()->GetString(
        prefs::kApplicationLocale));
  }
  return locale_;
}

void BrowserProcessImpl::CreateResourceDispatcherHost() {
  DCHECK(!created_resource_dispatcher_host_ &&
         resource_dispatcher_host_.get() == NULL);
  created_resource_dispatcher_host_ = true;

  resource_dispatcher_host_.reset(
      new ResourceDispatcherHost(io_thread()->message_loop()));
  resource_dispatcher_host_->Initialize();
}

void BrowserProcessImpl::CreateMetricsService() {
  DCHECK(!created_metrics_service_ && metrics_service_.get() == NULL);
  created_metrics_service_ = true;

  metrics_service_.reset(new MetricsService);
}

void BrowserProcessImpl::CreateIOThread() {
  DCHECK(!created_io_thread_ && io_thread_.get() == NULL);
  created_io_thread_ = true;

  // Prior to starting the io thread, we create the plugin service as
  // it is predominantly used from the io thread, but must be created
  // on the main thread. The service ctor is inexpensive and does not
  // invoke the io_thread() accessor.
  PluginService::GetInstance();

  scoped_ptr<base::Thread> thread(
      new BrowserProcessSubThread(ChromeThread::IO));
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  if (!thread->StartWithOptions(options))
    return;
  io_thread_.swap(thread);
}

void BrowserProcessImpl::CreateFileThread() {
  DCHECK(!created_file_thread_ && file_thread_.get() == NULL);
  created_file_thread_ = true;

  scoped_ptr<base::Thread> thread(
      new BrowserProcessSubThread(ChromeThread::FILE));
  base::Thread::Options options;
#if defined(OS_WIN)
  // On Windows, the FILE thread needs to be have a UI message loop which pumps
  // messages in such a way that Google Update can communicate back to us.
  options.message_loop_type = MessageLoop::TYPE_UI;
#else
  options.message_loop_type = MessageLoop::TYPE_IO;
#endif
  if (!thread->StartWithOptions(options))
    return;
  file_thread_.swap(thread);
}

void BrowserProcessImpl::CreateDBThread() {
  DCHECK(!created_db_thread_ && db_thread_.get() == NULL);
  created_db_thread_ = true;

  scoped_ptr<base::Thread> thread(
      new BrowserProcessSubThread(ChromeThread::DB));
  if (!thread->Start())
    return;
  db_thread_.swap(thread);
}

void BrowserProcessImpl::CreateProfileManager() {
  DCHECK(!created_profile_manager_ && profile_manager_.get() == NULL);
  created_profile_manager_ = true;

  profile_manager_.reset(new ProfileManager());
}

void BrowserProcessImpl::CreateLocalState() {
  DCHECK(!created_local_state_ && local_state_.get() == NULL);
  created_local_state_ = true;

  std::wstring local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  local_state_.reset(new PrefService(local_state_path));
}

void BrowserProcessImpl::InitBrokerServices(
    sandbox::BrokerServices* broker_services) {
  DCHECK(!initialized_broker_services_ && broker_services_ == NULL);
  broker_services->Init();
  initialized_broker_services_ = true;
  broker_services_ = broker_services;
}

void BrowserProcessImpl::CreateIconManager() {
  DCHECK(!created_icon_manager_ && icon_manager_.get() == NULL);
  created_icon_manager_ = true;
  icon_manager_.reset(new IconManager);
}

void BrowserProcessImpl::CreateDebuggerWrapper(int port) {
  DCHECK(debugger_wrapper_.get() == NULL);
  created_debugger_wrapper_ = true;

  debugger_wrapper_ = new DebuggerWrapper(port);
}

void BrowserProcessImpl::CreateAcceleratorHandler() {
#if defined(OS_WIN)
  DCHECK(accelerator_handler_.get() == NULL);
  scoped_ptr<views::AcceleratorHandler> accelerator_handler(
      new views::AcceleratorHandler);
  accelerator_handler_.swap(accelerator_handler);
#else
  // TODO(port): remove this completely, it has no business being here.
#endif
}

void BrowserProcessImpl::CreateGoogleURLTracker() {
  DCHECK(google_url_tracker_.get() == NULL);
  scoped_ptr<GoogleURLTracker> google_url_tracker(new GoogleURLTracker);
  google_url_tracker_.swap(google_url_tracker);
}

