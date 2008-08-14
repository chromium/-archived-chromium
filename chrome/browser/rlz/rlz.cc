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
#include "chrome/browser/template_url_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/google_update_settings.h"
//#include "chrome/common/pref_names.h"
//#include "chrome/common/pref_service.h"

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
                                    void* reserved);
}  // extern "C"

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
    return true;
  }
  return false;
}

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
    if (brand.empty())
      brand = L"GGLD";
    if (RLZTracker::SendFinancialPing(RLZTracker::CHROME, L"chrome",
                                      brand.c_str(), NULL, lang.c_str())) {
      access_values_state = ACCESS_VALUES_STALE;
    }
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
    if (!LoadRLZLibrary(directory_key_))
      return;
    // For non-interactive tests we don't do the rest of the initialization.
    if (::GetEnvironmentVariableW(env_vars::kHeadless, NULL, 0))
      return;
    if (first_run_) {
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
    }
    // Schedule the daily RLZ ping.
    Thread* thread = g_browser_process->file_thread();
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
    Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
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
  // Schedule the delayed init items.
  const int kOneHundredSeconds = 100000;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      new DelayedInitTask(directory_key, first_run), kOneHundredSeconds);
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

bool RLZTracker::SendFinancialPing(Product product,
                                   const wchar_t* product_signature,
                                   const wchar_t* product_brand,
                                   const wchar_t* product_id,
                                   const wchar_t* product_lang) {
  AccessPoint points[] = {CHROME_OMNIBOX, CHROME_HOME_PAGE, NO_ACCESS_POINT};
  return (send_ping) ? send_ping(product, points, product_signature,
      product_brand, product_id, product_lang, NULL) : false;
}
