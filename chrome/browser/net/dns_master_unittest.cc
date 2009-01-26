// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "base/spin_wait.h"
#include "base/timer.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/net/dns_host_info.h"
#include "chrome/common/net/dns.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/scoped_host_mapper.h"
#include "net/base/winsock_init.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace chrome_browser_net {

class WaitForResolutionHelper;

typedef base::RepeatingTimer<WaitForResolutionHelper> HelperTimer;

class WaitForResolutionHelper {
 public:
  WaitForResolutionHelper(DnsMaster* master, const NameList& hosts,
                          HelperTimer* timer)
      : master_(master),
        hosts_(hosts),
        timer_(timer) {
  }

  void Run() {
    for (NameList::const_iterator i = hosts_.begin(); i != hosts_.end(); ++i)
      if (master_->GetResolutionDuration(*i) == DnsHostInfo::kNullDuration)
        return;  // We don't have resolution for that host.

    // When all hostnames have been resolved, exit the loop.
    timer_->Stop();
    MessageLoop::current()->Quit();
    delete timer_;
    delete this;
  }

 private:
  DnsMaster* master_;
  const NameList hosts_;
  HelperTimer* timer_;
};

class DnsMasterTest : public testing::Test {
 protected:
  virtual void SetUp() {
#if defined(OS_WIN)
    net::EnsureWinsockInit();
#endif
    mapper_.AddRuleWithLatency("www.google.com", "127.0.0.1", 50);
    mapper_.AddRuleWithLatency("gmail.google.com.com", "127.0.0.1", 70);
    mapper_.AddRuleWithLatency("mail.google.com", "127.0.0.1", 44);
    mapper_.AddRuleWithLatency("gmail.com", "127.0.0.1", 63);

    mapper_.AddRuleWithLatency("veryslow.net", "127.0.0.1", 10000);
  }

  void WaitForResolution(DnsMaster* master, const NameList& hosts) {
    HelperTimer* timer = new HelperTimer();
    timer->Start(TimeDelta::FromMilliseconds(100),
                 new WaitForResolutionHelper(master, hosts, timer),
                 &WaitForResolutionHelper::Run);
    MessageLoop::current()->Run();
  }

 private:
  MessageLoop loop_;
  net::ScopedHostMapper mapper_;
};

//------------------------------------------------------------------------------
// Provide a function to create unique (nonexistant) domains at *every* call.
//------------------------------------------------------------------------------
static std::string GetNonexistantDomain() {
  static std::string postfix = ".google.com";
  static std::string prefix = "www.";
  static std::string mid = "datecount";

  static int counter = 0;  // Make sure its unique.
  time_t number = time(NULL);
  std::ostringstream result;
  result << prefix << number << mid << ++counter << postfix;
  return result.str();
}

//------------------------------------------------------------------------------
// Use a blocking function to contrast results we get via async services.
//------------------------------------------------------------------------------
TimeDelta BlockingDnsLookup(const std::string& hostname) {
    Time start = Time::Now();

    net::HostResolver resolver;
    net::AddressList addresses;
    resolver.Resolve(hostname, 80, &addresses, NULL);

    return Time::Now() - start;
}

//------------------------------------------------------------------------------

// First test to be sure the OS is caching lookups, which is the whole premise
// of DNS prefetching.
TEST_F(DnsMasterTest, OsCachesLookupsTest) {
  const Time start = Time::Now();
  int all_lookups = 0;
  int lookups_with_improvement = 0;
  // This test can be really flaky on Linux. It should run in much shorter time,
  // but sometimes it won't and we don't like bogus failures.
  while (Time::Now() - start < TimeDelta::FromMinutes(1)) {
    std::string badname;
    badname = GetNonexistantDomain();

    TimeDelta duration = BlockingDnsLookup(badname);

    // Produce more than one result and remove the largest one
    // to reduce flakiness.
    std::vector<TimeDelta> cached_results;
    for (int j = 0; j < 3; j++)
      cached_results.push_back(BlockingDnsLookup(badname));
    std::sort(cached_results.begin(), cached_results.end());
    cached_results.pop_back();

    TimeDelta cached_sum = TimeDelta::FromSeconds(0);
    for (std::vector<TimeDelta>::const_iterator j = cached_results.begin();
         j != cached_results.end(); ++j)
      cached_sum += *j;
    TimeDelta cached_duration = cached_sum / cached_results.size();

    all_lookups++;
    if (cached_duration < duration)
      lookups_with_improvement++;
    if (all_lookups >= 10)
      if (lookups_with_improvement * 100 > all_lookups * 75)
        // Okay, we see the improvement for more than 75% of all lookups.
        return;
  }
  FAIL() << "No substantial improvement in lookup time.";
}

TEST_F(DnsMasterTest, StartupShutdownTest) {
  DnsMaster testing_master;
  // We do shutdown on destruction.
}

TEST_F(DnsMasterTest, BenefitLookupTest) {
  DnsMaster testing_master;

  std::string goog("www.google.com"),
    goog2("gmail.google.com.com"),
    goog3("mail.google.com"),
    goog4("gmail.com");
  DnsHostInfo goog_info, goog2_info, goog3_info, goog4_info;

  // Simulate getting similar names from a network observer
  goog_info.SetHostname(goog);
  goog2_info.SetHostname(goog2);
  goog3_info.SetHostname(goog3);
  goog4_info.SetHostname(goog4);

  goog_info.SetStartedState();
  goog2_info.SetStartedState();
  goog3_info.SetStartedState();
  goog4_info.SetStartedState();

  goog_info.SetFinishedState(true);
  goog2_info.SetFinishedState(true);
  goog3_info.SetFinishedState(true);
  goog4_info.SetFinishedState(true);

  NameList names;
  names.insert(names.end(), goog);
  names.insert(names.end(), goog2);
  names.insert(names.end(), goog3);
  names.insert(names.end(), goog4);

  testing_master.ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(&testing_master, names);

  EXPECT_TRUE(testing_master.WasFound(goog));
  EXPECT_TRUE(testing_master.WasFound(goog2));
  EXPECT_TRUE(testing_master.WasFound(goog3));
  EXPECT_TRUE(testing_master.WasFound(goog4));

  // With the mock DNS, each of these should have taken some time, and hence
  // shown a benefit (i.e., prefetch cost more than network access time).

  // Simulate actual navigation, and acrue the benefit for "helping" the DNS
  // part of the navigation.
  EXPECT_TRUE(testing_master.AccruePrefetchBenefits(GURL(), &goog_info));
  EXPECT_TRUE(testing_master.AccruePrefetchBenefits(GURL(), &goog2_info));
  EXPECT_TRUE(testing_master.AccruePrefetchBenefits(GURL(), &goog3_info));
  EXPECT_TRUE(testing_master.AccruePrefetchBenefits(GURL(), &goog4_info));

  // Benefits can ONLY be reported once (for the first navigation).
  EXPECT_FALSE(testing_master.AccruePrefetchBenefits(GURL(), &goog_info));
  EXPECT_FALSE(testing_master.AccruePrefetchBenefits(GURL(), &goog2_info));
  EXPECT_FALSE(testing_master.AccruePrefetchBenefits(GURL(), &goog3_info));
  EXPECT_FALSE(testing_master.AccruePrefetchBenefits(GURL(), &goog4_info));
}

TEST_F(DnsMasterTest, ShutdownWhenResolutionIsPendingTest) {
  DnsMaster testing_master;

  std::string slow("veryslow.net");
  NameList names;
  names.insert(names.end(), slow);

  testing_master.ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(), 500);
  MessageLoop::current()->Run();
  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(testing_master.WasFound(slow));
}

TEST_F(DnsMasterTest, SingleLookupTest) {
  DnsMaster testing_master;

  std::string goog("www.google.com");

  NameList names;
  names.insert(names.end(), goog);

  // Try to flood the master with many concurrent requests.
  for (int i = 0; i < 10; i++)
    testing_master.ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(&testing_master, names);

  EXPECT_TRUE(testing_master.WasFound(goog));

  MessageLoop::current()->RunAllPending();

  EXPECT_GT(testing_master.peak_pending_lookups(), names.size() / 2);
  EXPECT_LE(testing_master.peak_pending_lookups(), names.size());
  EXPECT_LE(testing_master.peak_pending_lookups(),
            DnsMaster::kMaxConcurrentLookups);
}

TEST_F(DnsMasterTest, ConcurrentLookupTest) {
  DnsMaster testing_master;

  std::string goog("www.google.com"),
    goog2("gmail.google.com.com"),
    goog3("mail.google.com"),
    goog4("gmail.com");
  std::string bad1(GetNonexistantDomain()),
    bad2(GetNonexistantDomain());

  NameList names;
  names.insert(names.end(), goog);
  names.insert(names.end(), goog3);
  names.insert(names.end(), bad1);
  names.insert(names.end(), goog2);
  names.insert(names.end(), bad2);
  names.insert(names.end(), goog4);
  names.insert(names.end(), goog);

  // Warm up the *OS* cache for all the goog domains.
  BlockingDnsLookup(goog);
  BlockingDnsLookup(goog2);
  BlockingDnsLookup(goog3);
  BlockingDnsLookup(goog4);

  // Try to flood the master with many concurrent requests.
  for (int i = 0; i < 10; i++)
    testing_master.ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(&testing_master, names);

  EXPECT_TRUE(testing_master.WasFound(goog));
  EXPECT_TRUE(testing_master.WasFound(goog3));
  EXPECT_TRUE(testing_master.WasFound(goog2));
  EXPECT_TRUE(testing_master.WasFound(goog4));
  EXPECT_FALSE(testing_master.WasFound(bad1));
  EXPECT_FALSE(testing_master.WasFound(bad2));

  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(testing_master.WasFound(bad1));
  EXPECT_FALSE(testing_master.WasFound(bad2));

  EXPECT_GT(testing_master.peak_pending_lookups(), names.size() / 2);
  EXPECT_LE(testing_master.peak_pending_lookups(), names.size());
  EXPECT_LE(testing_master.peak_pending_lookups(),
            DnsMaster::kMaxConcurrentLookups);
}

TEST_F(DnsMasterTest, MassiveConcurrentLookupTest) {
  DnsMaster testing_master;

  NameList names;
  for (int i = 0; i < 100; i++)
    names.push_back(GetNonexistantDomain());

  // Try to flood the master with many concurrent requests.
  for (int i = 0; i < 10; i++)
    testing_master.ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(&testing_master, names);

  MessageLoop::current()->RunAllPending();

  EXPECT_LE(testing_master.peak_pending_lookups(), names.size());
  EXPECT_LE(testing_master.peak_pending_lookups(),
            DnsMaster::kMaxConcurrentLookups);
}

}  // namespace chrome_browser_net
