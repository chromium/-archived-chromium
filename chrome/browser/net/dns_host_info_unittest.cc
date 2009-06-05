// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Single threaded tests of DnsHostInfo functionality.

#include <time.h>
#include <string>

#include "base/platform_thread.h"
#include "chrome/browser/net/dns_host_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace {

class DnsHostInfoTest : public testing::Test {
};

typedef chrome_browser_net::DnsHostInfo DnsHostInfo;

TEST(DnsHostInfoTest, StateChangeTest) {
  DnsHostInfo info_practice, info;
  std::string hostname1("domain1.com"), hostname2("domain2.com");

  // First load DLL, so that their load time won't interfere with tests.
  // Some tests involve timing function performance, and DLL time can overwhelm
  // test durations (which are considering network vs cache response times).
  info_practice.SetHostname(hostname2);
  info_practice.SetQueuedState(DnsHostInfo::UNIT_TEST_MOTIVATED);
  info_practice.SetAssignedState();
  info_practice.SetFoundState();
  PlatformThread::Sleep(500);  // Allow time for DLLs to fully load.

  // Complete the construction of real test object.
  info.SetHostname(hostname1);

  EXPECT_TRUE(info.NeedsDnsUpdate(hostname1)) << "error in construction state";
  info.SetQueuedState(DnsHostInfo::UNIT_TEST_MOTIVATED);
  EXPECT_FALSE(info.NeedsDnsUpdate(hostname1))
    << "update needed after being queued";
  info.SetAssignedState();
  EXPECT_FALSE(info.NeedsDnsUpdate(hostname1));
  info.SetFoundState();
  EXPECT_FALSE(info.NeedsDnsUpdate(hostname1))
    << "default expiration time is TOOOOO short";

  // Note that time from ASSIGNED to FOUND was VERY short (probably 0ms), so the
  // object should conclude that no network activity was needed.  As a result,
  // the required time till expiration will be halved (guessing that we were
  // half way through having the cache expire when we did the lookup.
  EXPECT_LT(info.resolve_duration().InMilliseconds(),
    DnsHostInfo::kMaxNonNetworkDnsLookupDuration.InMilliseconds())
    << "Non-net time is set too low";

  info.set_cache_expiration(TimeDelta::FromMilliseconds(300));
  EXPECT_FALSE(info.NeedsDnsUpdate(hostname1)) << "expiration time not honored";
  PlatformThread::Sleep(80);  // Not enough time to pass our 300ms mark.
  EXPECT_FALSE(info.NeedsDnsUpdate(hostname1)) << "expiration time not honored";

  // That was a nice life when the object was found.... but next time it won't
  // be found.  We'll sleep for a while, and then come back with not-found.
  info.SetQueuedState(DnsHostInfo::UNIT_TEST_MOTIVATED);
  info.SetAssignedState();
  EXPECT_FALSE(info.NeedsDnsUpdate(hostname1));
  // Greater than minimal expected network latency on DNS lookup.
  PlatformThread::Sleep(25);
  info.SetNoSuchNameState();
  EXPECT_FALSE(info.NeedsDnsUpdate(hostname1))
    << "default expiration time is TOOOOO short";

  // Note that now we'll actually utilize an expiration of 300ms,
  // since there was detected network activity time during lookup.
  // We're assuming the caching just started with our lookup.
  PlatformThread::Sleep(80);  // Not enough time to pass our 300ms mark.
  EXPECT_FALSE(info.NeedsDnsUpdate(hostname1)) << "expiration time not honored";
  // Still not past our 300ms mark (only about 4+2ms)
  PlatformThread::Sleep(80);
  EXPECT_FALSE(info.NeedsDnsUpdate(hostname1)) << "expiration time not honored";
  PlatformThread::Sleep(150);
  EXPECT_TRUE(info.NeedsDnsUpdate(hostname1)) << "expiration time not honored";
}

// When a system gets "congested" relative to DNS, it means it is doing too many
// DNS resolutions, and bogging down the system.  When we detect such a
// situation, we divert the sequence of states a DnsHostInfo instance moves
// through.  Rather than proceeding from QUEUED (waiting in a name queue for a
// worker thread that can resolve the name) to ASSIGNED (where a worker thread
// actively resolves the name), we enter the ASSIGNED state (without actually
// getting sent to a resolver thread) and reset our state to what it was before
// the corresponding name was put in the work_queue_.  This test drives through
// the state transitions used in such congestion handling.
TEST(DnsHostInfoTest, CongestionResetStateTest) {
  DnsHostInfo info;
  std::string hostname1("domain1.com");

  info.SetHostname(hostname1);
  info.SetQueuedState(DnsHostInfo::UNIT_TEST_MOTIVATED);
  info.SetAssignedState();
  EXPECT_TRUE(info.is_assigned());

  info.RemoveFromQueue();  // Do the reset.
  EXPECT_FALSE(info.is_assigned());

  // Since this was a new info instance, and it never got resolved, we land back
  // in a PENDING state rather than FOUND or NO_SUCH_NAME.
  EXPECT_FALSE(info.was_found());
  EXPECT_FALSE(info.was_nonexistant());

  // Make sure we're completely re-usable, by going throug a normal flow.
  info.SetQueuedState(DnsHostInfo::UNIT_TEST_MOTIVATED);
  info.SetAssignedState();
  info.SetFoundState();
  EXPECT_TRUE(info.was_found());

  // Use the congestion flow, and check that we end up in the found state.
  info.SetQueuedState(DnsHostInfo::UNIT_TEST_MOTIVATED);
  info.SetAssignedState();
  info.RemoveFromQueue();  // Do the reset.
  EXPECT_FALSE(info.is_assigned());
  EXPECT_TRUE(info.was_found());  // Back to what it was before being queued.
}


// TODO(jar): Add death test for illegal state changes, and also for setting
// hostname when already set.

}  // namespace
