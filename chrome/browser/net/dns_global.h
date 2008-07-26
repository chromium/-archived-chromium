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
