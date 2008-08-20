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

#include "chrome/common/notification_service.h"

// TODO(evanm): This shouldn't depend on static initialization.
// static
TLSSlot NotificationService::tls_index_;

// static
bool NotificationService::HasKey(const NotificationSourceMap& map,
                                 const NotificationSource& source) {
  return map.find(source.map_key()) != map.end();
}

NotificationService::NotificationService() {
  DCHECK(current() == NULL);
#ifndef NDEBUG
  memset(observer_counts_, 0, sizeof(observer_counts_));
#endif

  tls_index_.Set(this);
}

void NotificationService::AddObserver(NotificationObserver* observer,
                                      NotificationType type,
                                      const NotificationSource& source) {
  DCHECK(type < NOTIFICATION_TYPE_COUNT);

  NotificationObserverList* observer_list;
  if (HasKey(observers_[type], source)) {
    observer_list = observers_[type][source.map_key()];
  } else {
    observer_list = new NotificationObserverList;
    observers_[type][source.map_key()] = observer_list;
  }

  observer_list->AddObserver(observer);
#ifndef NDEBUG
  ++observer_counts_[type];
#endif
}

void NotificationService::RemoveObserver(NotificationObserver* observer,
                                         NotificationType type,
                                         const NotificationSource& source) {
  DCHECK(type < NOTIFICATION_TYPE_COUNT);
  DCHECK(HasKey(observers_[type], source));

  NotificationObserverList* observer_list = observers_[type][source.map_key()];
  if (observer_list) {
    observer_list->RemoveObserver(observer);
#ifndef NDEBUG
    --observer_counts_[type];
#endif
  }

  // TODO(jhughes): Remove observer list from map if empty?
}

void NotificationService::Notify(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  DCHECK(type > NOTIFY_ALL);  // Allowed for subscription, but not posting.
  DCHECK(type < NOTIFICATION_TYPE_COUNT);

  // There's no particular reason for the order in which the different
  // classes of observers get notified here.

  // Notify observers of all types and all sources
  if (HasKey(observers_[NOTIFY_ALL], AllSources()) &&
      source != AllSources())
    FOR_EACH_OBSERVER(NotificationObserver,
                      *observers_[NOTIFY_ALL][AllSources().map_key()],
                      Observe(type, source, details));
  // Notify observers of all types and the given source
  if (HasKey(observers_[NOTIFY_ALL], source))
    FOR_EACH_OBSERVER(NotificationObserver,
                      *observers_[NOTIFY_ALL][source.map_key()],
                      Observe(type, source, details));
  // Notify observers of the given type and all sources
  if (HasKey(observers_[type], AllSources()) &&
      source != AllSources())
    FOR_EACH_OBSERVER(NotificationObserver,
                      *observers_[type][AllSources().map_key()],
                      Observe(type, source, details));
  // Notify observers of the given type and the given source
  if (HasKey(observers_[type], source))
    FOR_EACH_OBSERVER(NotificationObserver,
                      *observers_[type][source.map_key()],
                      Observe(type, source, details));
}


NotificationService::~NotificationService() {
  tls_index_.Set(NULL);

#ifndef NDEBUG
  for (int i = 0; i < NOTIFICATION_TYPE_COUNT; i++) {
    if (observer_counts_[i] > 0) {
      LOG(WARNING) << observer_counts_[i] << " notification observer(s) leaked"
          << " of notification type " << i;
    }
  }
#endif

  for (int i = 0; i < NOTIFICATION_TYPE_COUNT; i++) {
    NotificationSourceMap omap = observers_[i];
    for (NotificationSourceMap::iterator it = omap.begin();
         it != omap.end(); ++it) {
      delete it->second;
    }
  }
}

NotificationObserver::~NotificationObserver() {}
