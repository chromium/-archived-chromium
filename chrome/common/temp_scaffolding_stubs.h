// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
#define CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_

// This file provides declarations and stub definitions for classes we encouter
// during the porting effort. It is not meant to be perminent, and classes will
// be removed from here as they are fleshed out more completely.

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/browser_process.h"

class CommandLine;
class ProfileManager;
class Profile;
class MetricsService;

class Upgrade {
 public:
  static bool IsBrowserAlreadyRunning() { return false; }
  static bool RelaunchChromeBrowser(const CommandLine& command_line) {
    return true;
  }
  static bool SwapNewChromeExeIfPresent() { return true; }
};

class BrowserInit {
 public:
  // TODO(port): MessageWindow is very windows specific and shouldn't be part of
  // BrowserInit at all.
  class MessageWindow {
   public:
    explicit MessageWindow(const std::wstring& user_data_dir) { }
    ~MessageWindow() { }
    bool NotifyOtherProcess() { return false; }
    void HuntForZombieChromeProcesses() { }
    void Create() { }
    void Lock() { }
    void Unlock() { }
  };
  static bool ProcessCommandLine(const CommandLine& parsed_command_line,
                                 const std::wstring& cur_dir,
                                 PrefService* prefs, bool process_startup,
                                 Profile* profile, int* return_code);
  static bool LaunchBrowser(const CommandLine& parsed_command_line,
                            Profile* profile, const std::wstring& cur_dir,
                            bool process_startup, int* return_code);
 private:
  static bool LaunchBrowserImpl(const CommandLine& parsed_command_line,
                                Profile* profile, const std::wstring& cur_dir,
                                bool process_startup, int* return_code);
};

class FirstRun {
 public:
  static bool IsChromeFirstRun() { return false; }
  static bool ProcessMasterPreferences(const std::wstring& user_data_dir,
                                       const std::wstring& master_prefs_path,
                                       int* preference_details) {
    return false;
  }
  static int ImportNow(Profile* profile, const CommandLine& cmdline) {
    return 0;
  }
};

class GoogleUpdateSettings {
 public:
  static bool GetCollectStatsConsent() { return false; }
  static bool SetCollectStatsConsent(bool consented) { return false; }
  static bool GetBrowser(std::wstring* browser) { return false; }
  static bool GetLanguage(std::wstring* language) { return false; }
  static bool GetBrand(std::wstring* brand) { return false; }
  static bool GetReferral(std::wstring* referral) { return false; }
  static bool ClearReferral() { return false; }
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GoogleUpdateSettings);
};

class BrowserProcessImpl : public BrowserProcess {
 public:
  BrowserProcessImpl(CommandLine& command_line);
  virtual ~BrowserProcessImpl();

  virtual void EndSession() { }
  virtual ResourceDispatcherHost* resource_dispatcher_host() { return NULL; }
  virtual MetricsService* metrics_service();
  virtual ProfileManager* profile_manager();
  virtual PrefService* local_state();
  virtual DebuggerWrapper* debugger_wrapper() { return NULL; }
  virtual ClipboardService* clipboard_service() { return NULL; }
  virtual base::Thread* io_thread() { return NULL; }
  virtual base::Thread* file_thread() { return NULL; }
  virtual base::Thread* db_thread() { return NULL; }
  virtual sandbox::BrokerServices* broker_services() { return NULL; }
  virtual IconManager* icon_manager() { return NULL; }
  virtual void InitBrokerServices(sandbox::BrokerServices*) { }
  virtual AutomationProviderList* InitAutomationProviderList() { return NULL; }
  virtual void InitDebuggerWrapper(int port) { }
  virtual unsigned int AddRefModule() { return NULL; }
  virtual unsigned int ReleaseModule() { return NULL; }
  virtual bool IsShuttingDown() { return NULL; }
  virtual views::AcceleratorHandler* accelerator_handler() { return NULL; }
  virtual printing::PrintJobManager* print_job_manager() { return NULL; }
  virtual GoogleURLTracker* google_url_tracker() { return NULL; }
  virtual const std::wstring& GetApplicationLocale() { return locale_; }
  virtual MemoryModel memory_model() { return MEDIUM_MEMORY_MODEL; }
  virtual base::WaitableEvent* shutdown_event() { return NULL; }
 private:
  void CreateLocalState();
  void CreateProfileManager();
  void CreateMetricsService();

  bool created_local_state_;
  scoped_ptr<PrefService> local_state_;
  bool created_metrics_service_;
  scoped_ptr<MetricsService> metrics_service_;
  bool created_profile_manager_;
  scoped_ptr<ProfileManager> profile_manager_;
  std::wstring locale_;
};

class FirstRunBrowserProcess : public BrowserProcessImpl {
 public:
  FirstRunBrowserProcess(CommandLine& command_line)
      : BrowserProcessImpl(command_line) {
  }
  virtual ~FirstRunBrowserProcess() { }
  virtual GoogleURLTracker* google_url_tracker() { return NULL; }
 private:
  DISALLOW_COPY_AND_ASSIGN(FirstRunBrowserProcess);
};

class UserDataManager {
 public:
  static void Create();
  static UserDataManager* Get();

  explicit UserDataManager(const std::wstring& user_data_root) { }
 private:
  // Shared instance.
  static UserDataManager* instance_;
};

class Profile {
 public:
  Profile(const std::wstring& user_data_dir);
  virtual std::wstring GetPath() { return path_; }
  virtual PrefService* GetPrefs();
 private:
  std::wstring GetPrefFilePath();

  std::wstring path_;
  scoped_ptr<PrefService> prefs_;
};

class ProfileManager : NonThreadSafe {
 public:
  ProfileManager() { }
  virtual ~ProfileManager() { }
  Profile* GetDefaultProfile(const std::wstring& user_data_dir);
  static std::wstring GetDefaultProfileDir(const std::wstring& user_data_dir);
  static std::wstring GetDefaultProfilePath(const std::wstring& profile_dir);
 private:
  DISALLOW_EVIL_CONSTRUCTORS(ProfileManager);
};

class MetricsService {
 public:
  MetricsService() { }
  ~MetricsService() { }
  void Start() { }
  void StartRecordingOnly() { }
  void Stop() { }
  void SetUserPermitsUpload(bool enabled) { }
};

namespace browser {
void RegisterAllPrefs(PrefService*, PrefService*);
}

namespace browser_shutdown {
void ReadLastShutdownInfo();
void Shutdown();
}

void OpenFirstRunDialog(Profile* profile);

void InstallJankometer(const CommandLine&);

#endif  // CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
