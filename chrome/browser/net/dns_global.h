// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the global interface for the dns prefetch services.  It centralizes
// initialization, along with all the callbacks etc. to connect to the browser
// process.  This allows the more standard DNS prefetching services, such as
// provided by DnsMaster to be left as more generally usable code, and possibly
// be shared across multiple client projects.

#ifndef CHROME_BROWSER_NET_DNS_GLOBAL_H__
#define CHROME_BROWSER_NET_DNS_GLOBAL_H__

#include "chrome/browser/net/dns_master.h"

#include <string>

class PrefService;

namespace chrome_browser_net {

// Functions to initialize our global state so that PrefetchDns() can be called.
void InitDnsPrefetch(PrefService* user_prefs);
void ShutdownDnsPrefetch();

// Global API relating to Prefetching in browser
void EnableDnsPrefetch(bool enable);
void RegisterPrefs(PrefService* local_state);
void RegisterUserPrefs(PrefService* user_prefs);
void DnsPrefetchList(const NameList& hostnames);
void DnsPrefetchUrlString(const url_canon::UTF16String& url_string);
void DnsPrefetch(const std::string& hostname);
void DnsPrefetchGetHtmlInfo(std::string* output);

// Save the hostnames actually used at the start of this session to prefetch
// during the next startup.
void SaveHostNamesForNextStartup(PrefService* local_state);
void DnsPretchHostNamesAtStartup(PrefService* user_prefs,
                                 PrefService* local_state);

// Helper class to handle global init and shutdown.
class DnsPrefetcherInit {
 public:
  explicit DnsPrefetcherInit(PrefService* user_prefs) {
    InitDnsPrefetch(user_prefs);
  }
  ~DnsPrefetcherInit() {ShutdownDnsPrefetch();}

 private:
  DISALLOW_EVIL_CONSTRUCTORS(DnsPrefetcherInit);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_GLOBAL_H__

