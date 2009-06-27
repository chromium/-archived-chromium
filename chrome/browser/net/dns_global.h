// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the global interface for the dns prefetch services.  It centralizes
// initialization, along with all the callbacks etc. to connect to the browser
// process.  This allows the more standard DNS prefetching services, such as
// provided by DnsMaster to be left as more generally usable code, and possibly
// be shared across multiple client projects.

#ifndef CHROME_BROWSER_NET_DNS_GLOBAL_H_
#define CHROME_BROWSER_NET_DNS_GLOBAL_H_


#include <string>

#include "base/field_trial.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/net/dns_master.h"

class PrefService;

namespace net {
class HostResolver;
}

namespace chrome_browser_net {

// Initialize dns prefetching subsystem. Must be called before any other
// functions.
void InitDnsPrefetch(TimeDelta max_queue_delay, size_t max_concurrent,
                     PrefService* user_prefs);

// Cancel pending lookup requests and don't make new ones. Does nothing
// if dns prefetching has not been initialized (to simplify its usage).
void EnsureDnsPrefetchShutdown();

// Free all resources allocated by InitDnsPrefetch. After that you must not call
// any function from this file.
void FreeDnsPrefetchResources();

// Lazily allocates a HostResolver to be used by the DNS prefetch system, on
// the IO thread.
net::HostResolver* GetGlobalHostResolver();

//------------------------------------------------------------------------------
// Global APIs relating to Prefetching in browser
void EnableDnsPrefetch(bool enable);
void RegisterPrefs(PrefService* local_state);
void RegisterUserPrefs(PrefService* user_prefs);
// Renderer bundles up list and sends to this browser API via IPC.
void DnsPrefetchList(const NameList& hostnames);
// This API is used by the autocomplete popup box (as user types).
void DnsPrefetchUrl(const GURL& url);
void DnsPrefetchGetHtmlInfo(std::string* output);

//------------------------------------------------------------------------------
// Save the hostnames actually used at the start of this session to prefetch
// during the next startup.
void SaveHostNamesForNextStartup(PrefService* local_state);
void DnsPrefetchHostNamesAtStartup(PrefService* user_prefs,
                                   PrefService* local_state);

// Functions to save and restore sub-resource references.
void SaveSubresourceReferrers(PrefService* local_state);
void RestoreSubresourceReferrers(PrefService* local_state);
void TrimSubresourceReferrers();

//------------------------------------------------------------------------------
// Helper class to handle global init and shutdown.
class DnsPrefetcherInit {
 public:
  // Too many concurrent lookups negate benefits of prefetching by trashing
  // the OS cache before all resource loading is complete.
  // This is the default.
  static const size_t kMaxConcurrentLookups;

  // When prefetch requests are queued beyond some period of time, then the
  // system is congested, and we need to clear all queued requests to get out
  // of that state.  The following is the suggested default time limit.
  static const int kMaxQueueingDelayMs;

  DnsPrefetcherInit(PrefService* user_prefs, PrefService* local_state);
  ~DnsPrefetcherInit();

 private:
  // Maintain a field trial instance when we do A/B testing.
  scoped_refptr<FieldTrial> trial_;

  DISALLOW_COPY_AND_ASSIGN(DnsPrefetcherInit);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_GLOBAL_H_
