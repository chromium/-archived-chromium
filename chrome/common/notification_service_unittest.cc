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
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class NotificationServiceTest: public testing::Test {
};

class TestObserver : public NotificationObserver {
public:
  TestObserver() : notification_count_(0) {}

  int notification_count() { return notification_count_; }

  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    ++notification_count_;
  }

private:
  int notification_count_;
};

// Bogus class to act as a NotificationSource for the messages.
class TestSource {};

}  // namespace


TEST(NotificationServiceTest, Basic) {
  TestSource test_source;
  TestSource other_source;

  // Check the equality operators defined for NotificationSource
  EXPECT_TRUE(
    Source<TestSource>(&test_source) == Source<TestSource>(&test_source));
  EXPECT_TRUE(
    Source<TestSource>(&test_source) != Source<TestSource>(&other_source));

  TestObserver all_types_all_sources;
  TestObserver idle_all_sources;
  TestObserver all_types_test_source;
  TestObserver idle_test_source;

  NotificationService* service = NotificationService::current();

  // Make sure it doesn't freak out when there are no observers.
  service->Notify(NOTIFY_IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  service->AddObserver(
    &all_types_all_sources, NOTIFY_ALL, NotificationService::AllSources());
  service->AddObserver(
    &idle_all_sources, NOTIFY_IDLE, NotificationService::AllSources());
  service->AddObserver(
    &all_types_test_source, NOTIFY_ALL, Source<TestSource>(&test_source));
  service->AddObserver(
    &idle_test_source, NOTIFY_IDLE, Source<TestSource>(&test_source));

  EXPECT_EQ(0, all_types_all_sources.notification_count());
  EXPECT_EQ(0, idle_all_sources.notification_count());
  EXPECT_EQ(0, all_types_test_source.notification_count());
  EXPECT_EQ(0, idle_test_source.notification_count());

  service->Notify(NOTIFY_IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(1, all_types_all_sources.notification_count());
  EXPECT_EQ(1, idle_all_sources.notification_count());
  EXPECT_EQ(1, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(NOTIFY_BUSY,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(2, all_types_all_sources.notification_count());
  EXPECT_EQ(1, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(NOTIFY_IDLE,
                  Source<TestSource>(&other_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(3, all_types_all_sources.notification_count());
  EXPECT_EQ(2, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(NOTIFY_BUSY,
                  Source<TestSource>(&other_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(4, all_types_all_sources.notification_count());
  EXPECT_EQ(2, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  // Try send with NULL source.
  service->Notify(NOTIFY_IDLE,
                  NotificationService::AllSources(),
                  NotificationService::NoDetails());

  EXPECT_EQ(5, all_types_all_sources.notification_count());
  EXPECT_EQ(3, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->RemoveObserver(
    &all_types_all_sources, NOTIFY_ALL, NotificationService::AllSources());
  service->RemoveObserver(
    &idle_all_sources, NOTIFY_IDLE, NotificationService::AllSources());
  service->RemoveObserver(
    &all_types_test_source, NOTIFY_ALL, Source<TestSource>(&test_source));
  service->RemoveObserver(
    &idle_test_source, NOTIFY_IDLE, Source<TestSource>(&test_source));

  service->Notify(NOTIFY_IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(5, all_types_all_sources.notification_count());
  EXPECT_EQ(3, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  // Removing an observer that isn't there is a no-op, this should be fine.
  service->RemoveObserver(
    &all_types_all_sources, NOTIFY_ALL, NotificationService::AllSources());
}

TEST(NotificationServiceTest, MultipleRegistration) {
  TestSource test_source;

  TestObserver idle_test_source;

  NotificationService* service = NotificationService::current();

  service->AddObserver(
    &idle_test_source, NOTIFY_IDLE, Source<TestSource>(&test_source));
  service->AddObserver(
    &idle_test_source, NOTIFY_ALL, Source<TestSource>(&test_source));

  service->Notify(NOTIFY_IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());
  EXPECT_EQ(2, idle_test_source.notification_count());

  service->RemoveObserver(
    &idle_test_source, NOTIFY_IDLE, Source<TestSource>(&test_source));

  service->Notify(NOTIFY_IDLE,
                 Source<TestSource>(&test_source),
                 NotificationService::NoDetails());
  EXPECT_EQ(3, idle_test_source.notification_count());

  service->RemoveObserver(
    &idle_test_source, NOTIFY_ALL, Source<TestSource>(&test_source));

  service->Notify(NOTIFY_IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());
  EXPECT_EQ(3, idle_test_source.notification_count());
}
