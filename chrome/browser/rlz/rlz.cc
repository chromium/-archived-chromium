// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This code glues the RLZ library DLL with Chrome. It allows Chrome to work
// with or without the DLL being present. If the DLL is not present the
// functions do nothing and just return false.

#include "chrome/browser/rlz/rlz.h"

#include <windows.h>
#include <process.h>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/notification_service.h"
#include "chrome/installer/util/google_update_settings.h"

namespace {

// The maximum length of an access points RLZ in wide chars.
const DWORD kMaxRlzLength = 64;

// The RLZ is a DLL that might not be present in the system. We load it
// as needed but never unload it.
volatile HMODULE rlz_dll = NULL;

enum {
  ACCESS_VALUES_STALE,      // Possibly new values available.
  ACCESS_VALUES_FRESH       // The cached values are current.
};

// Tracks if we have tried and succeeded sending the ping. This helps us
// decide if we need to refresh the some cached strings.
volatile int access_values_state = ACCESS_VALUES_STALE;

extern "C" {
typedef bool (*RecordProductEventFn)(RLZTracker::Product product,
                                     RLZTracker::AccessPoint point,
                                     RLZTracker::Event event_id,
                                     void* reserved);

typedef bool (*GetAccessPointRlzFn)(RLZTracker::AccessPoint point,
                                    wchar_t* rlz,
                                    DWORD rlz_size,
                                    void* reserved);

typedef bool (*ClearAllProductEventsFn)(RLZTracker::Product product,
                                        void* reserved);

typedef bool (*SendFinancialPingFn)(RLZTracker::Product product,
                                    RLZTracker::AccessPoint* access_points,
                                    const WCHAR* product_signature,
                                    const WCHAR* product_brand,
                                    const WCHAR* product_id,
                                    const WCHAR* product_lang,
                                    bool exclude_id,
                                    void* reserved);
}  // extern "C".

RecordProductEventFn record_event = NULL;
GetAccessPointRlzFn get_access_point = NULL;
ClearAllProductEventsFn clear_all_events = NULL;
SendFinancialPingFn send_ping = NULL;

template <typename FuncT>
FuncT WireExport(HMODULE module, const char* export_name) {
  void* entry_point = ::GetProcAddress(module, export_name);
  return (module)? reinterpret_cast<FuncT>(entry_point) : NULL;
}

HMODULE LoadRLZLibraryInternal(int directory_key) {
  std::wstring rlz_path;
  if (!PathService::Get(directory_key, &rlz_path))
    return NULL;
  file_util::AppendToPath(&rlz_path, L"rlz.dll");
  return ::LoadLibraryW(rlz_path.c_str());
}

bool LoadRLZLibrary(int directory_key) {
  rlz_dll = LoadRLZLibraryInternal(directory_key);
  if (!rlz_dll) {
    // As a last resort we can try the EXE directory.
    if (directory_key != base::DIR_EXE)
      rlz_dll = LoadRLZLibraryInternal(base::DIR_EXE);
  }
  if (rlz_dll) {
    record_event =
        WireExport<RecordProductEventFn>(rlz_dll, "RecordProductEvent");
    get_access_point =
        WireExport<GetAccessPointRlzFn>(rlz_dll, "GetAccessPointRlz");
    clear_all_events =
        WireExport<ClearAllProductEventsFn>(rlz_dll, "ClearAllProductEvents");
    send_ping =
        WireExport<SendFinancialPingFn>(rlz_dll, "SendFinancialPing");
    return (record_event && get_access_point && clear_all_events && send_ping);
  }
  return false;
}

bool SendFinancialPing(const wchar_t* brand, const wchar_t* lang,
                       const wchar_t* referral, bool exclude_id) {
  RLZTracker::AccessPoint points[] = {RLZTracker::CHROME_OMNIBOX, 
                                      RLZTracker::CHROME_HOME_PAGE,
                                      RLZTracker::NO_ACCESS_POINT};
  if (!send_ping)
    return false; 
  return send_ping(RLZTracker::CHROME, points, L"chrome", brand, referral, lang,
                   exclude_id, NULL);
}

// This class leverages the AutocompleteEditModel notification to know when
// the user first interacted with the omnibox and set a global accordingly.
class OmniBoxUsageObserver : public NotificationObserver {
 public:
  OmniBoxUsageObserver() {
    NotificationService::current()->AddObserver(this,
        NOTIFY_OMNIBOX_OPENED_URL,
        NotificationService::AllSources());
    omnibox_used_ = false;
    DCHECK(!instance_);
    instance_ = this;
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    // Try to record event now, else set the flag to try later when we
    // attempt the ping.
    if (!RLZTracker::RecordProductEvent(RLZTracker::CHROME,
                                        RLZTracker::CHROME_OMNIBOX,
                                        RLZTracker::FIRST_SEARCH))       
      omnibox_used_ = true;
    delete this;
  }

  static bool used() {
    return omnibox_used_;
  }

  // Deletes the single instance of OmniBoxUsageObserver.
  static void DeleteInstance() {
    delete instance_;
  }

 private:
  // Dtor is private so the object cannot be created on the stack.
  ~OmniBoxUsageObserver() {
    NotificationService::current()->RemoveObserver(this,
        NOTIFY_OMNIBOX_OPENED_URL, 
        NotificationService::AllSources());
    instance_ = NULL;
  }

  static bool omnibox_used_;

  // There should only be one instance created at a time, and instance_ points
  // to that instance.
  // NOTE: this is only non-null for the amount of time it is needed. Once the
  // instance_ is no longer needed (or Chrome is exitting), this is null.
  static OmniBoxUsageObserver* instance_;
};

bool OmniBoxUsageObserver::omnibox_used_ = false;
OmniBoxUsageObserver* OmniBoxUsageObserver::instance_ = NULL;

// This task is run in the file thread, so to not block it for a long time
// we use a throwaway thread to do the blocking url request.
class DailyPingTask : public Task {
 public:
  virtual ~DailyPingTask() {
  }
  virtual void Run() {
    // We use a transient thread because we have no guarantees about
    // how long the RLZ lib can block us.
    _beginthread(PingNow, 0, NULL);
  }

 private:
  // Causes a ping to the server using WinInet. There is logic inside RLZ dll
  // that throttles it to a maximum of one ping per day.
  static void _cdecl PingNow(void*) {
    std::wstring lang;
    GoogleUpdateSettings::GetLanguage(&lang);
    if (lang.empty())
      lang = L"en";
    std::wstring brand;
    GoogleUpdateSettings::GetBrand(&brand);
    std::wstring referral;
    GoogleUpdateSettings::GetReferral(&referral);
    if (SendFinancialPing(brand.c_str(), lang.c_str(), referral.c_str(),
                          is_organic(brand))) {
      access_values_state = ACCESS_VALUES_STALE;
      GoogleUpdateSettings::ClearReferral();
    }
  }

  // Organic brands all start with GG, such as GGCM.
  static bool is_organic(const std::wstring brand) {
    return (brand.size() < 2) ? false : (brand.substr(0,2) == L"GG");
  }
};

// Performs late RLZ initialization and RLZ event recording for chrome.
// This task needs to run on the UI thread.
class DelayedInitTask : public Task {
 public:
  explicit DelayedInitTask(int directory_key, bool first_run)
      : directory_key_(directory_key), first_run_(first_run) {
  }
  virtual ~DelayedInitTask() {
  }
  virtual void Run() {
    // For non-interactive tests we don't do the rest of the initialization
    // because sometimes the very act of loading the dll causes QEMU to crash.
    if (::GetEnvironmentVariableW(env_vars::kHeadless, NULL, 0))
      return;
    if (!LoadRLZLibrary(directory_key_))
      return;
    // Do the initial event recording if is the first run or if we have an
    // empty rlz which means we haven't got a chance to do it.
    std::wstring omnibox_rlz;
    RLZTracker::GetAccessPointRlz(RLZTracker::CHROME_OMNIBOX, &omnibox_rlz);

    if (first_run_ || omnibox_rlz.empty()) {
      // Record the installation of chrome.
      RLZTracker::RecordProductEvent(RLZTracker::CHROME,
                                     RLZTracker::CHROME_OMNIBOX,
                                     RLZTracker::INSTALL);
      RLZTracker::RecordProductEvent(RLZTracker::CHROME,
                                     RLZTracker::CHROME_HOME_PAGE,
                                     RLZTracker::INSTALL);
      // Record if google is the initial search provider.
      if (IsGoogleDefaultSearch()) {
        RLZTracker::RecordProductEvent(RLZTracker::CHROME,
                                       RLZTracker::CHROME_OMNIBOX,
                                       RLZTracker::SET_TO_GOOGLE);
      }
      // Record first user interaction with the omnibox.
      if (OmniBoxUsageObserver::used()) {
        RLZTracker::RecordProductEvent(RLZTracker::CHROME,
                                       RLZTracker::CHROME_OMNIBOX,
                                       RLZTracker::FIRST_SEARCH);       
      }
    }
    // Schedule the daily RLZ ping.
    base::Thread* thread = g_browser_process->file_thread();
    if (thread)
      thread->message_loop()->PostTask(FROM_HERE, new DailyPingTask());
  }

 private:
  bool IsGoogleDefaultSearch() {
    if (!g_browser_process)
      return false;
    std::wstring user_data_dir;
    if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
      return false;
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    Profile* profile = profile_manager->
        GetDefaultProfile(FilePath::FromWStringHack(user_data_dir));
    if (!profile)
      return false;
    const TemplateURL* url_template =
        profile->GetTemplateURLModel()->GetDefaultSearchProvider();
    if (!url_template)
      return false;
    return url_template->url()->HasGoogleBaseURLs();
  }

  int directory_key_;
  bool first_run_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(DelayedInitTask);
};

}  // namespace

bool RLZTracker::InitRlz(int directory_key) {
  return LoadRLZLibrary(directory_key);
}

bool RLZTracker::InitRlzDelayed(int directory_key, bool first_run) {
  if (!OmniBoxUsageObserver::used())
    new OmniBoxUsageObserver();
  // Schedule the delayed init items.
  const int kNinetySeconds = 90 * 1000;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      new DelayedInitTask(directory_key, first_run), kNinetySeconds);
  return true;
}

bool RLZTracker::RecordProductEvent(Product product, AccessPoint point,
                                    Event event) {
  return (record_event) ? record_event(product, point, event, NULL) : false;
}

bool RLZTracker::ClearAllProductEvents(Product product) {
  return (clear_all_events) ? clear_all_events(product, NULL) : false;
}

// We implement caching of the answer of get_access_point() if the request
// is for CHROME_OMNIBOX. If we had a successful ping, then we update the
// cached value.

bool RLZTracker::GetAccessPointRlz(AccessPoint point, std::wstring* rlz) {
  static std::wstring cached_ommibox_rlz;
  if (!get_access_point)
    return false;
  if ((CHROME_OMNIBOX == point) &&
      (access_values_state == ACCESS_VALUES_FRESH)) {
    *rlz = cached_ommibox_rlz;
    return true;
  }
  wchar_t str_rlz[kMaxRlzLength];
  if (!get_access_point(point, str_rlz, kMaxRlzLength, NULL))
    return false;
  if (CHROME_OMNIBOX == point) {
    access_values_state = ACCESS_VALUES_FRESH;
    cached_ommibox_rlz.assign(str_rlz);
  }
  *rlz = str_rlz;
  return true;
}

// static
void RLZTracker::CleanupRlz() {
  OmniBoxUsageObserver::DeleteInstance();
}
