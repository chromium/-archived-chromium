// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_global.h"

#include <map>
#include <string>

#include "base/singleton.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_host_info.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "net/base/host_resolver.h"

using base::TimeDelta;

namespace chrome_browser_net {

static void DiscardAllPrefetchState();
static void DnsMotivatedPrefetch(const std::string& hostname,
                                 DnsHostInfo::ResolutionMotivation motivation);
static void DnsPrefetchMotivatedList(
    const NameList& hostnames,
    DnsHostInfo::ResolutionMotivation motivation);

// static
const size_t DnsPrefetcherInit::kMaxConcurrentLookups = 8;

// Host resolver shared by DNS prefetcher, and the main URLRequestContext.
static net::HostResolver* global_host_resolver = NULL;

//------------------------------------------------------------------------------
// This section contains all the globally accessable API entry points for the
// DNS Prefetching feature.
//------------------------------------------------------------------------------

// Status of prefetch feature, controlling whether any prefetching is done.
static bool dns_prefetch_enabled = true;

// Cached inverted copy of the off_the_record pref.
static bool on_the_record_switch = true;

// Enable/disable Dns prefetch activity (either via command line, or via pref).
void EnableDnsPrefetch(bool enable) {
  dns_prefetch_enabled = enable;
}

void OnTheRecord(bool enable) {
  if (on_the_record_switch == enable)
    return;
  on_the_record_switch = enable;
  if (on_the_record_switch)
    DiscardAllPrefetchState();  // Destroy all evidence of our OTR session.
}

void RegisterPrefs(PrefService* local_state) {
  local_state->RegisterListPref(prefs::kDnsStartupPrefetchList);
  local_state->RegisterListPref(prefs::kDnsHostReferralList);
}

void RegisterUserPrefs(PrefService* user_prefs) {
  user_prefs->RegisterBooleanPref(prefs::kDnsPrefetchingEnabled, true);
}

// When enabled, we use the following instance to service all requests in the
// browser process.
static DnsMaster* dns_master;

// This API is only used in the browser process.
// It is called from an IPC message originating in the renderer.  It currently
// includes both Page-Scan, and Link-Hover prefetching.
// TODO(jar): Separate out link-hover prefetching, and page-scan results.
void DnsPrefetchList(const NameList& hostnames) {
  DnsPrefetchMotivatedList(hostnames, DnsHostInfo::PAGE_SCAN_MOTIVATED);
}

static void DnsPrefetchMotivatedList(
    const NameList& hostnames,
    DnsHostInfo::ResolutionMotivation motivation) {
  if (!dns_prefetch_enabled || NULL == dns_master)
    return;
  dns_master->ResolveList(hostnames, motivation);
}

// This API is used by the autocomplete popup box (where URLs are typed).
void DnsPrefetchUrl(const GURL& url) {
  if (!dns_prefetch_enabled  || NULL == dns_master)
    return;
  if (url.is_valid())
    DnsMotivatedPrefetch(url.host(), DnsHostInfo::OMNIBOX_MOTIVATED);
}

static void DnsMotivatedPrefetch(const std::string& hostname,
                                 DnsHostInfo::ResolutionMotivation motivation) {
  if (!dns_prefetch_enabled || NULL == dns_master || !hostname.size())
    return;
  dns_master->Resolve(hostname, motivation);
}

//------------------------------------------------------------------------------
// This section intermingles prefetch results with actual browser HTTP
// network activity.  It supports calculating of the benefit of a prefetch, as
// well as recording what prefetched hostname resolutions might be potentially
// helpful during the next chrome-startup.
//------------------------------------------------------------------------------

// This function determines if there was a saving by prefetching the hostname
// for which the navigation_info is supplied.
static bool AccruePrefetchBenefits(const GURL& referrer,
                                   DnsHostInfo* navigation_info) {
  if (!dns_prefetch_enabled || NULL == dns_master)
    return false;
  return dns_master->AccruePrefetchBenefits(referrer, navigation_info);
}

// When we navigate, we may know in advance some other domains that will need to
// be resolved.  This function initiates those side effects.
static void NavigatingTo(const std::string& host_name) {
  if (!dns_prefetch_enabled || NULL == dns_master)
    return;
  dns_master->NavigatingTo(host_name);
}

// The observer class needs to connect starts and finishes of HTTP network
// resolutions.  We use the following type for that map.
typedef std::map<int, DnsHostInfo> ObservedResolutionMap;

// There will only be one instance ever created of the following Observer
// class.  As a result, we get away with using static members for data local
// to that instance (to better comply with a google style guide exemption).
class PrefetchObserver : public net::HostResolver::Observer {
 public:
  PrefetchObserver();
  ~PrefetchObserver();

  virtual void OnStartResolution(
      int request_id,
      const net::HostResolver::RequestInfo& request_info);
  virtual void OnFinishResolutionWithStatus(
      int request_id,
      bool was_resolved,
      const net::HostResolver::RequestInfo& request_info);
  virtual void OnCancelResolution(
      int request_id,
      const net::HostResolver::RequestInfo& request_info);

  static void DnsGetFirstResolutionsHtml(std::string* output);
  static void SaveStartupListAsPref(PrefService* local_state);

 private:
  static void StartupListAppend(const DnsHostInfo& navigation_info);

  // We avoid using member variables to better comply with the style guide.
  // We had permission to instantiate only a very minimal class as a global
  // data item, so we avoid putting members in that class.
  // There is really only one instance of this class, and it would have been
  // much simpler to use member variables than these static members.
  static Lock* lock;
  // Map of pending resolutions seen by observer.
  static ObservedResolutionMap* resolutions;
  // List of the first N hostname resolutions observed in this run.
  static Results* first_resolutions;
  // The number of hostnames we'll save for prefetching at next startup.
  static const size_t kStartupResolutionCount = 10;
};

//------------------------------------------------------------------------------
// Member definitions for above Observer class.

PrefetchObserver::PrefetchObserver() {
  DCHECK(!lock && !resolutions && !first_resolutions);
  lock = new Lock;
  resolutions = new ObservedResolutionMap;
  first_resolutions = new Results;
}

PrefetchObserver::~PrefetchObserver() {
  DCHECK(lock && resolutions && first_resolutions);
  delete first_resolutions;
  first_resolutions = NULL;
  delete resolutions;
  resolutions = NULL;
  delete lock;
  lock = NULL;
}

void PrefetchObserver::OnStartResolution(
    int request_id,
    const net::HostResolver::RequestInfo& request_info) {
  if (request_info.is_speculative())
    return;  // One of our own requests.
  DCHECK_NE(0U, request_info.hostname().length());
  DnsHostInfo navigation_info;
  navigation_info.SetHostname(request_info.hostname());
  navigation_info.SetStartedState();

  NavigatingTo(request_info.hostname());

  AutoLock auto_lock(*lock);
  // This entry will be deleted either by OnFinishResolutionWithStatus(), or
  // by  OnCancelResolution().
  (*resolutions)[request_id] = navigation_info;
}

void PrefetchObserver::OnFinishResolutionWithStatus(
    int request_id,
    bool was_resolved,
    const net::HostResolver::RequestInfo& request_info) {
  if (request_info.is_speculative())
    return;  // One of our own requests.
  DnsHostInfo navigation_info;
  size_t startup_count;
  {
    AutoLock auto_lock(*lock);
    ObservedResolutionMap::iterator it = resolutions->find(request_id);
    if (resolutions->end() == it) {
      DCHECK(false);
      return;
    }
    navigation_info = it->second;
    resolutions->erase(it);
    startup_count = first_resolutions->size();
  }
  navigation_info.SetFinishedState(was_resolved);  // Get timing info
  AccruePrefetchBenefits(request_info.referrer(), &navigation_info);
  if (kStartupResolutionCount <= startup_count || !was_resolved)
    return;
  // TODO(jar): Don't add host to our list if it is a non-linked lookup, and
  // instead rely on Referrers to pull this in automatically with the enclosing
  // page load (once we start to persist elements of our referrer tree).
  StartupListAppend(navigation_info);
}

void PrefetchObserver::OnCancelResolution(
    int request_id,
    const net::HostResolver::RequestInfo& request_info) {
  if (request_info.is_speculative())
    return;  // One of our own requests.

  // Remove the entry from |resolutions| that was added by OnStartResolution().
  AutoLock auto_lock(*lock);
  ObservedResolutionMap::iterator it = resolutions->find(request_id);
  if (resolutions->end() == it) {
    DCHECK(false);
    return;
  }
  resolutions->erase(it);
}

// static
void PrefetchObserver::StartupListAppend(const DnsHostInfo& navigation_info) {
  if (!on_the_record_switch || NULL == dns_master)
    return;
  AutoLock auto_lock(*lock);
  if (kStartupResolutionCount <= first_resolutions->size())
    return;  // Someone just added the last item.
  std::string host_name = navigation_info.hostname();
  if (first_resolutions->find(host_name) != first_resolutions->end())
    return;  // We already have this hostname listed.
  (*first_resolutions)[host_name] = navigation_info;
}

// static
void PrefetchObserver::SaveStartupListAsPref(PrefService* local_state) {
  ListValue* startup_list =
      local_state->GetMutableList(prefs::kDnsStartupPrefetchList);

  DCHECK(startup_list);
  if (!startup_list)
    return;
  startup_list->Clear();
  DCHECK(startup_list->GetSize() == 0);
  AutoLock auto_lock(*lock);
  for (Results::iterator it = first_resolutions->begin();
       it != first_resolutions->end();
       it++) {
    const std::string hostname = it->first;
    startup_list->Append(Value::CreateStringValue(hostname));
  }
}

// static
void PrefetchObserver::DnsGetFirstResolutionsHtml(std::string* output) {
  DnsHostInfo::DnsInfoTable resolution_list;
  {
    AutoLock auto_lock(*lock);
    for (Results::iterator it(first_resolutions->begin());
         it != first_resolutions->end();
         it++) {
      resolution_list.push_back(it->second);
    }
  }
  DnsHostInfo::GetHtmlTable(resolution_list,
      "Future startups will prefetch DNS records for ", false, output);
}

// static
Lock* PrefetchObserver::lock = NULL;
// static
ObservedResolutionMap* PrefetchObserver::resolutions = NULL;
// static
Results* PrefetchObserver::first_resolutions = NULL;

//------------------------------------------------------------------------------
// Support observer to detect opening and closing of OffTheRecord windows.

class OffTheRecordObserver : public NotificationObserver {
 public:
  void Register() {
    // TODO(pkasting): This test should not be necessary.  See crbug.com/12475.
    if (registrar_.IsEmpty()) {
      registrar_.Add(this, NotificationType::BROWSER_CLOSED,
                     NotificationService::AllSources());
      registrar_.Add(this, NotificationType::BROWSER_OPENED,
                     NotificationService::AllSources());
    }
  }

  void Observe(NotificationType type, const NotificationSource& source,
               const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::BROWSER_OPENED:
        if (!Source<Browser>(source)->profile()->IsOffTheRecord())
          break;
        {
          AutoLock lock(lock_);
          ++count_off_the_record_windows_;
        }
        OnTheRecord(false);
        break;

      case NotificationType::BROWSER_CLOSED:
        if (!Source<Browser>(source)->profile()->IsOffTheRecord())
          break;  // Ignore ordinary windows.
        {
          AutoLock lock(lock_);
          DCHECK(0 < count_off_the_record_windows_);
          if (0 >= count_off_the_record_windows_)  // Defensive coding.
            break;
          if (--count_off_the_record_windows_)
            break;  // Still some windows are incognito.
        }  // Release lock.
        OnTheRecord(true);
        break;

      default:
        break;
    }
  }

 private:
  friend struct DefaultSingletonTraits<OffTheRecordObserver>;
  OffTheRecordObserver() : lock_(), count_off_the_record_windows_(0) { }
  ~OffTheRecordObserver() { }

  NotificationRegistrar registrar_;
  Lock lock_;
  int count_off_the_record_windows_;

  DISALLOW_COPY_AND_ASSIGN(OffTheRecordObserver);
};

//------------------------------------------------------------------------------
// This section supports the about:dns page.
//------------------------------------------------------------------------------

// Provide global support for the about:dns page.
void DnsPrefetchGetHtmlInfo(std::string* output) {
  output->append("<html><head><title>About DNS</title>"
                 // We'd like the following no-cache... but it doesn't work.
                 // "<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">"
                 "</head><body>");
  if (!dns_prefetch_enabled  || NULL == dns_master) {
    output->append("Dns Prefetching is disabled.");
  } else {
    if (!on_the_record_switch) {
      output->append("Incognito mode is active in a window.");
    } else {
      dns_master->GetHtmlInfo(output);
      PrefetchObserver::DnsGetFirstResolutionsHtml(output);
      dns_master->GetHtmlReferrerLists(output);
    }
  }
  output->append("</body></html>");
}

//------------------------------------------------------------------------------
// This section intializes and tears down global DNS prefetch services.
//------------------------------------------------------------------------------

// Note: We have explicit permission to create the following global static
// object (in opposition to Google style rules). By making it a static, we
// can ensure its deletion.
static PrefetchObserver dns_resolution_observer;

void InitDnsPrefetch(size_t max_concurrent, PrefService* user_prefs) {
  // Use a large shutdown time so that UI tests (that instigate lookups, and
  // then try to shutdown the browser) don't instigate the CHECK about
  // "some slaves have not finished"
  const TimeDelta kAllowableShutdownTime(TimeDelta::FromSeconds(10));
  DCHECK(NULL == dns_master);
  if (!dns_master) {
    // Have the DnsMaster issue resolve requests through a global HostResolver
    // that is shared by the main URLRequestContext, and lives on the IO thread.
    dns_master = new DnsMaster(GetGlobalHostResolver(),
                               g_browser_process->io_thread()->message_loop(),
                               max_concurrent);
    dns_master->AddRef();
    // We did the initialization, so we should prime the pump, and set up
    // the DNS resolution system to run.
    Singleton<OffTheRecordObserver>::get()->Register();

    if (user_prefs) {
      bool enabled = user_prefs->GetBoolean(prefs::kDnsPrefetchingEnabled);
      EnableDnsPrefetch(enabled);
    }

    DLOG(INFO) << "DNS Prefetch service started";

    // Start observing real HTTP stack resolutions.
    // TODO(eroman): really this should be called from IO thread (since that is
    // where the host resolver lives). Since this occurs before requests have
    // started it is not a race yet.
    GetGlobalHostResolver()->AddObserver(&dns_resolution_observer);
  }
}

void EnsureDnsPrefetchShutdown() {
  if (NULL != dns_master) {
    dns_master->Shutdown();

    // Stop observing DNS resolutions. Note that dns_master holds a reference
    // to the global host resolver, so is guaranteed to be live.
    GetGlobalHostResolver()->RemoveObserver(&dns_resolution_observer);
  }

  // TODO(eroman): This is a hack so the in process browser tests work if
  // BrowserMain() is to be called again.
  global_host_resolver = NULL;
}

void FreeDnsPrefetchResources() {
  DCHECK(NULL != dns_master);
  dns_master->Release();
  dns_master = NULL;
}

static void DiscardAllPrefetchState() {
  if (!dns_master)
    return;
  dns_master->DiscardAllResults();
}

//------------------------------------------------------------------------------

net::HostResolver* GetGlobalHostResolver() {
  // Called from UI thread.
  if (!global_host_resolver) {
    static const size_t kMaxHostCacheEntries = 100;
    static const size_t kHostCacheExpirationSeconds = 60;  // 1 minute.

    global_host_resolver = new net::HostResolver(
        kMaxHostCacheEntries, kHostCacheExpirationSeconds * 1000);
  }
  return global_host_resolver;
}

//------------------------------------------------------------------------------
// Functions to handle saving of hostnames from one session to the next, to
// expedite startup times.

void SaveHostNamesForNextStartup(PrefService* local_state) {
  if (!dns_prefetch_enabled)
    return;
  PrefetchObserver::SaveStartupListAsPref(local_state);
}

void DnsPrefetchHostNamesAtStartup(PrefService* user_prefs,
                                   PrefService* local_state) {
  NameList hostnames;
  // Prefetch DNS for hostnames we learned about during last session.
  // This may catch secondary hostnames, pulled in by the homepages.  It will
  // also catch more of the "primary" home pages, since that was (presumably)
  // rendered first (and will be rendered first this time too).
  ListValue* startup_list =
      local_state->GetMutableList(prefs::kDnsStartupPrefetchList);
  if (startup_list) {
    for (ListValue::iterator it = startup_list->begin();
         it != startup_list->end();
         it++) {
      std::string hostname;
      (*it)->GetAsString(&hostname);
      hostnames.push_back(hostname);
    }
  }

  // Prepare for any static home page(s) the user has in prefs.  The user may
  // have a LOT of tab's specified, so we may as well try to warm them all.
  SessionStartupPref tab_start_pref =
      SessionStartupPref::GetStartupPref(user_prefs);
  if (SessionStartupPref::URLS == tab_start_pref.type) {
    for (size_t i = 0; i < tab_start_pref.urls.size(); i++) {
      GURL gurl = tab_start_pref.urls[i];
      if (gurl.is_valid() && !gurl.host().empty())
        hostnames.push_back(gurl.host());
    }
  }

  if (hostnames.size() > 0)
    DnsPrefetchMotivatedList(hostnames, DnsHostInfo::STARTUP_LIST_MOTIVATED);
  else  // Start a thread.
    DnsMotivatedPrefetch(std::string("www.google.com"),
                         DnsHostInfo::STARTUP_LIST_MOTIVATED);
}

//------------------------------------------------------------------------------
// Functions to persist and restore host references, that are used to direct DNS
// prefetch of names (probably) used in subresources when the major resource is
// navigated towards.

void SaveSubresourceReferrers(PrefService* local_state) {
  if (NULL == dns_master)
    return;
  ListValue* referral_list =
      local_state->GetMutableList(prefs::kDnsHostReferralList);
  dns_master->SerializeReferrers(referral_list);
}

void RestoreSubresourceReferrers(PrefService* local_state) {
  if (NULL == dns_master)
    return;
  ListValue* referral_list =
      local_state->GetMutableList(prefs::kDnsHostReferralList);
  dns_master->DeserializeReferrers(*referral_list);
}

void TrimSubresourceReferrers() {
  if (NULL == dns_master)
    return;
  dns_master->TrimReferrers();
}

}  // namespace chrome_browser_net
