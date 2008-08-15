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

#include "chrome/test/test_notification_tracker.h"
#include "chrome/common/notification_types.h"

TestNotificationTracker::Event::Event()
    : type(NOTIFY_ALL),
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
