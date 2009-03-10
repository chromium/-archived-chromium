// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/test_notification_tracker.h"
#include "chrome/common/notification_service.h"

TestNotificationTracker::Event::Event()
    : type(NotificationType::ALL),
      source(NotificationService::AllSources()),
      details(NotificationService::NoDetails()) {
}
TestNotificationTracker::Event::Event(NotificationType t,
                                      NotificationSource s,
                                      NotificationDetails d)
    : type(t),
      source(s),
      details(d) {
}

TestNotificationTracker::TestNotificationTracker() {
}

TestNotificationTracker::~TestNotificationTracker() {
  NotificationService* service = NotificationService::current();
  for (size_t i = 0; i < listening_.size(); i++)
    service->RemoveObserver(this, listening_[i].first, listening_[i].second);
  listening_.clear();
}

void TestNotificationTracker::ListenFor(NotificationType type,
                                        const NotificationSource& source) {
  listening_.push_back(std::make_pair(type, source));
  NotificationService::current()->AddObserver(this, type, source);
}

void TestNotificationTracker::Reset() {
  events_.clear();
}

bool TestNotificationTracker::Check1AndReset(NotificationType type) {
  if (size() != 1) {
    Reset();
    return false;
  }
  bool success = events_[0].type == type;
  Reset();
  return success;
}

bool TestNotificationTracker::Check2AndReset(NotificationType type1,
                                             NotificationType type2) {
  if (size() != 2) {
    Reset();
    return false;
  }
  bool success = events_[0].type == type1 && events_[1].type == type2;
  Reset();
  return success;
}

bool TestNotificationTracker::Check3AndReset(NotificationType type1,
                                             NotificationType type2,
                                             NotificationType type3) {
  if (size() != 3) {
    Reset();
    return false;
  }
  bool success = events_[0].type == type1 &&
                 events_[1].type == type2 &&
                 events_[2].type == type3;
  Reset();
  return success;
}

void TestNotificationTracker::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  events_.push_back(Event(type, source, details));
}
