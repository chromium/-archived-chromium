// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_NOTIFICATION_TRACKER_H_
#define CHROME_TEST_NOTIFICATION_TRACKER_H_

#include <vector>

#include "chrome/common/notification_service.h"

// Provides an easy way for tests to verify that a given set of notifications
// was received during test execution.
class TestNotificationTracker : public NotificationObserver {
 public:
  // Records one received notification.
  struct Event {
    Event();
    Event(NotificationType t, NotificationSource s, NotificationDetails d);

    NotificationType type;
    NotificationSource source;
    NotificationDetails details;
  };

  // By default, it won't listen for any notifications. You'll need to call
  // ListenFor for the notifications you are interested in.
  TestNotificationTracker();

  ~TestNotificationTracker();

  // Makes this object listen for the given notification with the given source.
  void ListenFor(NotificationType type, const NotificationSource& source);

  // Makes this object listen for notifications of the given type coming from
  // any source.
  void ListenForAll(NotificationType type);

  // Clears the list of events.
  void Reset();

  // Given notifications type(sp, returns true if the list of notifications
  // were exactly those listed in the given arg(s), and in the same order.
  //
  // This will also reset the list so that the next call will only check for
  // new notifications. Example:
  //   <do stuff>
  //   Check1AndReset(NOTIFY_A);
  //   <do stuff>
  //   Check2AndReset(NOTIFY_B, NOTIFY_C)
  bool Check1AndReset(NotificationType type);
  bool Check2AndReset(NotificationType type1,
                      NotificationType type2);
  bool Check3AndReset(NotificationType type1,
                      NotificationType type2,
                      NotificationType type3);

  // Returns the number of notifications received since the last reset.
  size_t size() const { return events_.size(); }

  // Returns the information about the event at the given index. The index must
  // be in [0, size).
  const Event& at(size_t i) const { return events_[i]; }

 private:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Lists all type/source combinations that we're listening for. These will
  // need to be unregistered when we are destroyed.
  std::vector< std::pair<NotificationType, NotificationSource> > listening_;

  // Lists all received since last cleared, in the order they were received.
  std::vector<Event> events_;

  DISALLOW_COPY_AND_ASSIGN(TestNotificationTracker);
};

#endif  // CHROME_TEST_NOTIFICATION_TRACKER_H_
