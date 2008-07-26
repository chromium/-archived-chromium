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

#ifndef CHROME_BROWSER_CHROME_PLUGIN_BROWSING_CONTEXT_H__
#define CHROME_BROWSER_CHROME_PLUGIN_BROWSING_CONTEXT_H__

#include <map>

#include "base/id_map.h"
#include "chrome/common/chrome_plugin_api.h"
#include "chrome/common/notification_service.h"

class URLRequestContext;

// This class manages the mapping between CPBrowsingContexts and
// URLRequestContexts.  It observes when URLRequestContexts go away, and
// invalidates the corresponding CPBrowsingContexts.  CPBrowsingContexts can be
// associated with other data as well, so there can be multiple ones referring
// to a given URLRequestContext.
// Note: This class should be used on the IO thread only.
class CPBrowsingContextManager : public NotificationObserver {
 public:
  static CPBrowsingContextManager* Instance();

  // Note: don't call these directly - use Instance() above.  They are public
  // so Singleton can access them.
  CPBrowsingContextManager();
  ~CPBrowsingContextManager();

  // Generate a new unique CPBrowsingContext ID from the given
  // URLRequestContext.  Multiple CPBrowsingContexts can map to the same
  // URLRequestContext.
  CPBrowsingContext Allocate(URLRequestContext* context);

  // Return the URLRequestContext that this CPBrowsingContext refers to, or NULL
  // if not found.
  URLRequestContext* ToURLRequestContext(CPBrowsingContext id);

  // Return a CPBrowsingContext ID that corresponds to the given
  // URLRequestContext. This function differs from Allocate in that calling
  // this multiple times with the same argument gives the same ID.
  CPBrowsingContext Lookup(URLRequestContext* context);

 private:
  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  typedef IDMap<URLRequestContext> Map;
  typedef std::map<URLRequestContext*, CPBrowsingContext> ReverseMap;

  Map map_;  // map of CPBrowsingContext -> URLRequestContext
  ReverseMap reverse_map_;  // map of URLRequestContext -> CPBrowsingContext
};

#endif  // CHROME_BROWSER_CHROME_PLUGIN_BROWSING_CONTEXT_H__
