// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_global.h"

#include <map>
#include <string>

#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_host_info.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/common/notification_types.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"
#include "net/base/dns_resolution_observer.h"

using base::TimeDelta;

namespace chrome_browser_net {

static void DiscardAllPrefetchState();
static void DnsMotivatedPrefetch(const std::string& hostname,
                                 DnsHostInfo::ResolutionMotivation motivation);
static void DnsPrefetchMotivatedList(
    const NameList& hostnames,
    DnsHostInfo::ResolutionMotivation motivation);

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
// TODO(jar): Separate out link-hover prefetching, and histogram results
// separately.
void DnsPrefetchList(const NameList& hostnames) {
  DnsPrefetchMotivatedList(hostnames, DnsHostInfo::PAGE_SCAN_MOTIVATED);
}

static void DnsPrefetchMotivatedList(
    const NameList& hostnames,
    DnsHostInfo::ResolutionMotivation motivation) {
  if (!dns_prefetch_enabled)
    return;
  DCHECK(NULL != dns_master);
  if (NULL != dns_master)
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
typedef std::map<void*, DnsHostInfo> ObservedResolutionMap;

// There will only be one instance ever created of the following Observer
// class.  As a result, we get away with using static members for data local
// to that instance (to better comply with a google style guide exemption).
class PrefetchObserver : public net::DnsResolutionObserver {
 public:
  PrefetchObserver();
  ~PrefetchObserver();

  virtual void OnStartResolution(const std::string& host_name,
                                 void* context);
  virtual void OnFinishResolutionWithStatus(bool was_resolved,
                                            const GURL& referrer,
                                            void* context);

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

void PrefetchObserver::OnStartResolution(const std::string& host_name,
                                         void* context) {
  DCHECK_NE(0U, host_name.length());
  DnsHostInfo navigation_info;
  navigation_info.SetHostname(host_name);
  navigation_info.SetStartedState();

  NavigatingTo(host_name);

  AutoLock auto_lock(*lock);
  (*resolutions)[context] = navigation_info;
}

void PrefetchObserver::OnFinishResolutionWithStatus(bool was_resolved,
                                                    const GURL& referrer,
                                                    void* context) {
  DnsHostInfo navigation_info;
  size_t startup_count;
  {
    AutoLock auto_lock(*lock);
    ObservedResolutionMap::iterator it = resolutions->find(context);
    if (resolutions->end() == it) {
      DCHECK(false);
      return;
    }
    navigation_info = it->second;
    resolutions->erase(it);
    startup_count = first_resolutions->size();
  }
  navigation_info.SetFinishedState(was_resolved);  // Get timing info
  AccruePrefetchBenefits(referrer, &navigation_info);
  if (kStartupResolutionCount <= startup_count || !was_resolved)
    return;
  // TODO(jar): Don't add host to our list if it is a non-linked lookup, and
  // instead rely on Referrers to pull this in automatically with the enclosing
  // page load.
  StartupListAppend(navigation_info);
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
  OffTheRecordObserver() : lock_(), count_off_the_record_windows_(0) { }

  ~OffTheRecordObserver() { }

  // Register as an observer, and rely on the NotificationSystem shutdown
  // to unregister us (at the last possible moment).
  void Register() {
    NotificationService* service = NotificationService::current();
    // TODO(tc): These notification observers are never removed.
    service->AddObserver(this, NOTIFY_BROWSER_CLOSED,
                         NotificationService::AllSources());
    service->AddObserver(this, NOTIFY_BROWSER_OPENED,
                         NotificationService::AllSources());
  }

  void Observe(NotificationType type, const NotificationSource& source,
               const NotificationDetails& details) {
    switch (type) {
      case NOTIFY_BROWSER_OPENED:
        if (!Source<Browser>(source)->profile()->IsOffTheRecord())
          break;
        {
          AutoLock lock(lock_);
          ++count_off_the_record_windows_;
        }
        OnTheRecord(false);
        break;

      case NOTIFY_BROWSER_CLOSED:
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
  Lock lock_;
  int count_off_the_record_windows_;

  DISALLOW_COPY_AND_ASSIGN(OffTheRecordObserver);
};

// TODO(jar): Use static class object so that I don't have to get the
// destruction time right (which requires unregistering just before the
// notification-service shuts down).
static OffTheRecordObserver off_the_record_observer;

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

void InitDnsPrefetch(PrefService* user_prefs) {
  // Use a large shutdown time so that UI tests (that instigate lookups, and
  // then try to shutdown the browser) don't instigate the CHECK about
  // "some slaves have not finished"
  const TimeDelta kAllowableShutdownTime(TimeDelta::FromSeconds(10));
  DCHECK(NULL == dns_master);
  if (!dns_master) {
    dns_master = new DnsMaster(kAllowableShutdownTime);
    // We did the initialization, so we should prime the pump, and set up
    // the DNS resolution system to run.
    off_the_record_observer.Register();

    if (user_prefs) {
      bool enabled = user_prefs->GetBoolean(prefs::kDnsPrefetchingEnabled);
      EnableDnsPrefetch(enabled);
    }

    DLOG(INFO) << "DNS Prefetch service started";

    // Start observing real HTTP stack resolutions.
    net::AddDnsResolutionObserver(&dns_resolution_observer);
  }
}

void ShutdownDnsPrefetch() {
  DCHECK(NULL != dns_master);
  DnsMaster* master = dns_master;
  dns_master = NULL;
  if (master->ShutdownSlaves()) {
    delete master;
  } else {
    // Leak instance if shutdown problem.
    DCHECK(0);
  }
}

static void DiscardAllPrefetchState() {
  if (!dns_master)
    return;
  dns_master->DiscardAllResults();
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


}  // namespace chrome_browser_net

