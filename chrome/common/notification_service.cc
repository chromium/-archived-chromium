// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/notification_service.h"

#include "base/lazy_instance.h"
#include "base/thread_local.h"

static base::LazyInstance<base::ThreadLocalPointer<NotificationService> >
    lazy_tls_ptr(base::LINKER_INITIALIZED);

// static
NotificationService* NotificationService::current() {
  return lazy_tls_ptr.Pointer()->Get();
}

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

  lazy_tls_ptr.Pointer()->Set(this);
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
  lazy_tls_ptr.Pointer()->Set(NULL);

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

