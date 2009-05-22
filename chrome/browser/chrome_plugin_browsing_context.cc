// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_plugin_browsing_context.h"

#include "base/message_loop.h"
#include "base/singleton.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/notification_service.h"

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
  registrar_.Add(this, NotificationType::URL_REQUEST_CONTEXT_RELEASED,
                 NotificationService::AllSources());
}

CPBrowsingContextManager::~CPBrowsingContextManager() {
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
  DCHECK(type == NotificationType::URL_REQUEST_CONTEXT_RELEASED);

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
