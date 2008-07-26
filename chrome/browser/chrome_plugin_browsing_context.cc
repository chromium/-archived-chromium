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

#include "chrome/browser/chrome_plugin_browsing_context.h"

#include "base/message_loop.h"
#include "base/singleton.h"
#include "chrome/browser/chrome_thread.h"

CPBrowsingContextManager* CPBrowsingContextManager::Instance() {
#ifndef NDEBUG
  // IO loop is NULL in unit tests.
  if (ChromeThread::GetMessageLoop(ChromeThread::IO)) {
    DCHECK(MessageLoop::current() ==
           ChromeThread::GetMessageLoop(ChromeThread::IO));
  }
#endif
  return Singleton<CPBrowsingContextManager>::get();
}

CPBrowsingContextManager::CPBrowsingContextManager() {
  NotificationService::current()->AddObserver(
      this, NOTIFY_URL_REQUEST_CONTEXT_RELEASED,
      NotificationService::AllSources());
}

CPBrowsingContextManager::~CPBrowsingContextManager() {
  // We don't remove ourselves as an observer because we are a Singleton object,
  // and NotifcationService is likely gone by this point.
}

CPBrowsingContext CPBrowsingContextManager::Allocate(
    URLRequestContext* context) {
  int32 map_id = map_.Add(context);
  return static_cast<CPBrowsingContext>(map_id);
}

URLRequestContext* CPBrowsingContextManager::ToURLRequestContext(
    CPBrowsingContext id) {
  return map_.Lookup(static_cast<int32>(id));
}

CPBrowsingContext CPBrowsingContextManager::Lookup(URLRequestContext* context) {
  ReverseMap::const_iterator it = reverse_map_.find(context);
  if (it == reverse_map_.end()) {
    CPBrowsingContext id = Allocate(context);
    reverse_map_[context] = id;
    return id;
  } else {
    return it->second;
  }
}

void CPBrowsingContextManager::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  DCHECK(type == NOTIFY_URL_REQUEST_CONTEXT_RELEASED);

  URLRequestContext* context = Source<URLRequestContext>(source).ptr();

  // Multiple CPBrowsingContexts may refer to the same URLRequestContext.
  // Remove after collecting all entries, since removing may invalidate
  // iterators.
  std::vector<int32> ids_to_remove;
  for (Map::const_iterator it = map_.begin(); it != map_.end(); ++it) {
    if (it->second == context)
      ids_to_remove.push_back(it->first);
  }

  for (size_t i = 0; i < ids_to_remove.size(); ++i) {
    map_.Remove(ids_to_remove[i]);
  }

  reverse_map_.erase(context);
}