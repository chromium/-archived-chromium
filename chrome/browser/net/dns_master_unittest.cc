// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include <algorithm>
#include <sstream>
#include <string>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/net/dns_host_info.h"
#include "chrome/common/net/dns.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_unittest.h"
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
 public:
  DnsMasterTest()
      : mapper_(new net::RuleBasedHostMapper()),
        scoped_mapper_(mapper_.get()) {
  }

 protected:
  virtual void SetUp() {
#if defined(OS_WIN)
    net::EnsureWinsockInit();
#endif
    mapper_->AddRuleWithLatency("www.google.com", "127.0.0.1", 50);
    mapper_->AddRuleWithLatency("gmail.google.com.com", "127.0.0.1", 70);
    mapper_->AddRuleWithLatency("mail.google.com", "127.0.0.1", 44);
    mapper_->AddRuleWithLatency("gmail.com", "127.0.0.1", 63);
  }

  void WaitForResolution(DnsMaster* master, const NameList& hosts) {
    HelperTimer* timer = new HelperTimer();
    timer->Start(TimeDelta::FromMilliseconds(100),
                 new WaitForResolutionHelper(master, hosts, timer),
                 &WaitForResolutionHelper::Run);
    MessageLoop::current()->Run();
  }

  net::HostResolver host_resolver_;
  scoped_refptr<net::RuleBasedHostMapper> mapper_;

 private:
  MessageLoop loop;
  net::ScopedHostMapper scoped_mapper_;
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
    net::HostResolver::RequestInfo info(hostname, 80);
    resolver.Resolve(info, &addresses, NULL, NULL);

    return Time::Now() - start;
}

//------------------------------------------------------------------------------

// First test to be sure the OS is caching lookups, which is the whole premise
// of DNS prefetching.
TEST_F(DnsMasterTest, OsCachesLookupsTest) {
  mapper_->AllowDirectLookup("*.google.com");

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
  scoped_refptr<DnsMaster> testing_master = new DnsMaster(&host_resolver_,
      MessageLoop::current(), DnsPrefetcherInit::kMaxConcurrentLookups);
  testing_master->Shutdown();
}

TEST_F(DnsMasterTest, BenefitLookupTest) {
  scoped_refptr<DnsMaster> testing_master = new DnsMaster(&host_resolver_,
      MessageLoop::current(), DnsPrefetcherInit::kMaxConcurrentLookups);

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

  testing_master->ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(testing_master, names);

  EXPECT_TRUE(testing_master->WasFound(goog));
  EXPECT_TRUE(testing_master->WasFound(goog2));
  EXPECT_TRUE(testing_master->WasFound(goog3));
  EXPECT_TRUE(testing_master->WasFound(goog4));

  // With the mock DNS, each of these should have taken some time, and hence
  // shown a benefit (i.e., prefetch cost more than network access time).

  // Simulate actual navigation, and acrue the benefit for "helping" the DNS
  // part of the navigation.
  EXPECT_TRUE(testing_master->AccruePrefetchBenefits(GURL(), &goog_info));
  EXPECT_TRUE(testing_master->AccruePrefetchBenefits(GURL(), &goog2_info));
  EXPECT_TRUE(testing_master->AccruePrefetchBenefits(GURL(), &goog3_info));
  EXPECT_TRUE(testing_master->AccruePrefetchBenefits(GURL(), &goog4_info));

  // Benefits can ONLY be reported once (for the first navigation).
  EXPECT_FALSE(testing_master->AccruePrefetchBenefits(GURL(), &goog_info));
  EXPECT_FALSE(testing_master->AccruePrefetchBenefits(GURL(), &goog2_info));
  EXPECT_FALSE(testing_master->AccruePrefetchBenefits(GURL(), &goog3_info));
  EXPECT_FALSE(testing_master->AccruePrefetchBenefits(GURL(), &goog4_info));

  testing_master->Shutdown();
}

TEST_F(DnsMasterTest, ShutdownWhenResolutionIsPendingTest) {
  scoped_refptr<net::WaitingHostMapper> mapper = new net::WaitingHostMapper();
  net::ScopedHostMapper scoped_mapper(mapper.get());

  scoped_refptr<DnsMaster> testing_master = new DnsMaster(&host_resolver_,
      MessageLoop::current(), DnsPrefetcherInit::kMaxConcurrentLookups);

  std::string localhost("127.0.0.1");
  NameList names;
  names.insert(names.end(), localhost);

  testing_master->ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(), 500);
  MessageLoop::current()->Run();

  EXPECT_FALSE(testing_master->WasFound(localhost));

  testing_master->Shutdown();

  // Clean up after ourselves.
  mapper->Signal();
  MessageLoop::current()->RunAllPending();
}

TEST_F(DnsMasterTest, SingleLookupTest) {
  scoped_refptr<DnsMaster> testing_master = new DnsMaster(&host_resolver_,
      MessageLoop::current(), DnsPrefetcherInit::kMaxConcurrentLookups);

  std::string goog("www.google.com");

  NameList names;
  names.insert(names.end(), goog);

  // Try to flood the master with many concurrent requests.
  for (int i = 0; i < 10; i++)
    testing_master->ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(testing_master, names);

  EXPECT_TRUE(testing_master->WasFound(goog));

  MessageLoop::current()->RunAllPending();

  EXPECT_GT(testing_master->peak_pending_lookups(), names.size() / 2);
  EXPECT_LE(testing_master->peak_pending_lookups(), names.size());
  EXPECT_LE(testing_master->peak_pending_lookups(),
            DnsPrefetcherInit::kMaxConcurrentLookups);

  testing_master->Shutdown();
}

TEST_F(DnsMasterTest, ConcurrentLookupTest) {
  mapper_->AddSimulatedFailure("*.notfound");

  scoped_refptr<DnsMaster> testing_master = new DnsMaster(&host_resolver_,
      MessageLoop::current(), DnsPrefetcherInit::kMaxConcurrentLookups);

  std::string goog("www.google.com"),
    goog2("gmail.google.com.com"),
    goog3("mail.google.com"),
    goog4("gmail.com");
  std::string bad1("bad1.notfound"),
    bad2("bad2.notfound");

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
    testing_master->ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(testing_master, names);

  EXPECT_TRUE(testing_master->WasFound(goog));
  EXPECT_TRUE(testing_master->WasFound(goog3));
  EXPECT_TRUE(testing_master->WasFound(goog2));
  EXPECT_TRUE(testing_master->WasFound(goog4));
  EXPECT_FALSE(testing_master->WasFound(bad1));
  EXPECT_FALSE(testing_master->WasFound(bad2));

  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(testing_master->WasFound(bad1));
  EXPECT_FALSE(testing_master->WasFound(bad2));

  EXPECT_GT(testing_master->peak_pending_lookups(), names.size() / 2);
  EXPECT_LE(testing_master->peak_pending_lookups(), names.size());
  EXPECT_LE(testing_master->peak_pending_lookups(),
            DnsPrefetcherInit::kMaxConcurrentLookups);

  testing_master->Shutdown();
}

TEST_F(DnsMasterTest, DISABLED_MassiveConcurrentLookupTest) {
  mapper_->AddSimulatedFailure("*.notfound");

  scoped_refptr<DnsMaster> testing_master = new DnsMaster(&host_resolver_,
      MessageLoop::current(), DnsPrefetcherInit::kMaxConcurrentLookups);

  NameList names;
  for (int i = 0; i < 100; i++)
    names.push_back("host" + IntToString(i) + ".notfound");

  // Try to flood the master with many concurrent requests.
  for (int i = 0; i < 10; i++)
    testing_master->ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(testing_master, names);

  MessageLoop::current()->RunAllPending();

  EXPECT_LE(testing_master->peak_pending_lookups(), names.size());
  EXPECT_LE(testing_master->peak_pending_lookups(),
            DnsPrefetcherInit::kMaxConcurrentLookups);

  testing_master->Shutdown();
}

//------------------------------------------------------------------------------
// Functions to help synthesize and test serializations of subresource referrer
// lists.

// Return a motivation_list if we can find one for the given motivating_host (or
// NULL if a match is not found).
static ListValue* FindSerializationMotivation(const std::string& motivation,
                                              const ListValue& referral_list) {
  ListValue* motivation_list(NULL);
  for (size_t i = 0; i < referral_list.GetSize(); ++i) {
    referral_list.GetList(i, &motivation_list);
    std::string existing_motivation;
    EXPECT_TRUE(motivation_list->GetString(i, &existing_motivation));
    if (existing_motivation == motivation)
      break;
    motivation_list = NULL;
  }
  return motivation_list;
}

// Add a motivating_host and a subresource_host to a serialized list, using
// this given latency. This is a helper function for quickly building these
// lists.
static void AddToSerializedList(const std::string& motivation,
                                const std::string& subresource, int latency,
                                ListValue* referral_list ) {
  // Find the motivation if it is already used.
  ListValue* motivation_list = FindSerializationMotivation(motivation,
                                                           *referral_list);
  if (!motivation_list) {
    // This is the first mention of this motivation, so build a list.
    motivation_list = new ListValue;
    motivation_list->Append(new StringValue(motivation));
    // Provide empty subresource list.
    motivation_list->Append(new ListValue());

    // ...and make it part of the serialized referral_list.
    referral_list->Append(motivation_list);
  }

  ListValue* subresource_list(NULL);
  EXPECT_TRUE(motivation_list->GetList(1, &subresource_list));

  // We won't bother to check for the subresource being there already.  Worst
  // case, during deserialization, the latency value we supply plus the
  // existing value(s) will be added to the referrer.
  subresource_list->Append(new StringValue(subresource));
  subresource_list->Append(new FundamentalValue(latency));
}

static const int kLatencyNotFound = -1;

// For a given motivation_hostname, and subresource_hostname, find what latency
// is currently listed.  This assume a well formed serialization, which has
// at most one such entry for any pair of names.  If no such pair is found, then
// return kLatencyNotFound.
int GetLatencyFromSerialization(const std::string& motivation,
                                const std::string& subresource,
                                const ListValue& referral_list) {
  ListValue* motivation_list = FindSerializationMotivation(motivation,
                                                           referral_list);
  if (!motivation_list)
    return kLatencyNotFound;
  ListValue* subresource_list;
  EXPECT_TRUE(motivation_list->GetList(1, &subresource_list));
  for (size_t i = 0; i < subresource_list->GetSize(); ++i) {
    std::string subresource_name;
    EXPECT_TRUE(subresource_list->GetString(i, &subresource_name));
    if (subresource_name == subresource) {
      int latency;
      EXPECT_TRUE(subresource_list->GetInteger(i + 1, &latency));
      return latency;
    }
    ++i;  // Skip latency value.
  }
  return kLatencyNotFound;
}

//------------------------------------------------------------------------------

// Make sure nil referral lists really have no entries, and no latency listed.
TEST_F(DnsMasterTest, ReferrerSerializationNilTest) {
  scoped_refptr<DnsMaster> master = new DnsMaster(&host_resolver_,
      MessageLoop::current(), DnsPrefetcherInit::kMaxConcurrentLookups);
  ListValue referral_list;
  master->SerializeReferrers(&referral_list);
  EXPECT_EQ(0U, referral_list.GetSize());
  EXPECT_EQ(kLatencyNotFound, GetLatencyFromSerialization("a.com", "b.com",
                                                          referral_list));

  master->Shutdown();
}

// Make sure that when a serialization list includes a value, that it can be
// deserialized into the database, and can be extracted back out via
// serialization without being changed.
TEST_F(DnsMasterTest, ReferrerSerializationSingleReferrerTest) {
  scoped_refptr<DnsMaster> master = new DnsMaster(&host_resolver_,
      MessageLoop::current(), DnsPrefetcherInit::kMaxConcurrentLookups);
  std::string motivation_hostname = "www.google.com";
  std::string subresource_hostname = "icons.google.com";
  const int kLatency = 3;
  ListValue referral_list;

  AddToSerializedList(motivation_hostname, subresource_hostname, kLatency,
                      &referral_list);

  master->DeserializeReferrers(referral_list);

  ListValue recovered_referral_list;
  master->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(1U, recovered_referral_list.GetSize());
  EXPECT_EQ(kLatency, GetLatencyFromSerialization(motivation_hostname,
                                                  subresource_hostname,
                                                  recovered_referral_list));

  master->Shutdown();
}

// Make sure the Trim() functionality works as expected.
TEST_F(DnsMasterTest, ReferrerSerializationTrimTest) {
  scoped_refptr<DnsMaster> master = new DnsMaster(&host_resolver_,
      MessageLoop::current(), DnsPrefetcherInit::kMaxConcurrentLookups);
  std::string motivation_hostname = "www.google.com";
  std::string icon_subresource_hostname = "icons.google.com";
  std::string img_subresource_hostname = "img.google.com";
  ListValue referral_list;

  AddToSerializedList(motivation_hostname, icon_subresource_hostname, 10,
                      &referral_list);
  AddToSerializedList(motivation_hostname, img_subresource_hostname, 3,
                      &referral_list);

  master->DeserializeReferrers(referral_list);

  ListValue recovered_referral_list;
  master->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(1U, recovered_referral_list.GetSize());
  EXPECT_EQ(10, GetLatencyFromSerialization(motivation_hostname,
                                            icon_subresource_hostname,
                                            recovered_referral_list));
  EXPECT_EQ(3, GetLatencyFromSerialization(motivation_hostname,
                                            img_subresource_hostname,
                                            recovered_referral_list));

  // Each time we Trim, the latency figures should reduce by a factor of two,
  // until they both are 0, an then a trim will delete the whole entry.
  master->TrimReferrers();
  master->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(1U, recovered_referral_list.GetSize());
  EXPECT_EQ(5, GetLatencyFromSerialization(motivation_hostname,
                                            icon_subresource_hostname,
                                            recovered_referral_list));
  EXPECT_EQ(1, GetLatencyFromSerialization(motivation_hostname,
                                            img_subresource_hostname,
                                            recovered_referral_list));

  master->TrimReferrers();
  master->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(1U, recovered_referral_list.GetSize());
  EXPECT_EQ(2, GetLatencyFromSerialization(motivation_hostname,
                                            icon_subresource_hostname,
                                            recovered_referral_list));
  EXPECT_EQ(0, GetLatencyFromSerialization(motivation_hostname,
                                            img_subresource_hostname,
                                            recovered_referral_list));

  master->TrimReferrers();
  master->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(1U, recovered_referral_list.GetSize());
  EXPECT_EQ(1, GetLatencyFromSerialization(motivation_hostname,
                                           icon_subresource_hostname,
                                           recovered_referral_list));
  EXPECT_EQ(0, GetLatencyFromSerialization(motivation_hostname,
                                           img_subresource_hostname,
                                           recovered_referral_list));

  master->TrimReferrers();
  master->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(0U, recovered_referral_list.GetSize());
  EXPECT_EQ(kLatencyNotFound,
            GetLatencyFromSerialization(motivation_hostname,
                                        icon_subresource_hostname,
                                        recovered_referral_list));
  EXPECT_EQ(kLatencyNotFound,
            GetLatencyFromSerialization(motivation_hostname,
                                        img_subresource_hostname,
                                        recovered_referral_list));

  master->Shutdown();
}


TEST_F(DnsMasterTest, PriorityQueuePushPopTest) {
  DnsMaster::HostNameQueue queue;

  // First check high priority queue FIFO functionality.
  EXPECT_TRUE(queue.IsEmpty());
  queue.Push("a", DnsHostInfo::LEARNED_REFERAL_MOTIVATED);
  EXPECT_FALSE(queue.IsEmpty());
  queue.Push("b", DnsHostInfo::MOUSE_OVER_MOTIVATED);
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.Pop(), "a");
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.Pop(), "b");
  EXPECT_TRUE(queue.IsEmpty());

  // Then check low priority queue FIFO functionality.
  queue.IsEmpty();
  queue.Push("a", DnsHostInfo::PAGE_SCAN_MOTIVATED);
  EXPECT_FALSE(queue.IsEmpty());
  queue.Push("b", DnsHostInfo::OMNIBOX_MOTIVATED);
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.Pop(), "a");
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.Pop(), "b");
  EXPECT_TRUE(queue.IsEmpty());
}

TEST_F(DnsMasterTest, PriorityQueueReorderTest) {
  DnsMaster::HostNameQueue queue;

  // Push all the low priority items.
  EXPECT_TRUE(queue.IsEmpty());
  queue.Push("scan", DnsHostInfo::PAGE_SCAN_MOTIVATED);
  queue.Push("unit", DnsHostInfo::UNIT_TEST_MOTIVATED);
  queue.Push("lmax", DnsHostInfo::LINKED_MAX_MOTIVATED);
  queue.Push("omni", DnsHostInfo::OMNIBOX_MOTIVATED);
  queue.Push("startup", DnsHostInfo::STARTUP_LIST_MOTIVATED);
  queue.Push("omni", DnsHostInfo::OMNIBOX_MOTIVATED);

  // Push all the high prority items
  queue.Push("learned", DnsHostInfo::LEARNED_REFERAL_MOTIVATED);
  queue.Push("refer", DnsHostInfo::STATIC_REFERAL_MOTIVATED);
  queue.Push("mouse", DnsHostInfo::MOUSE_OVER_MOTIVATED);

  // Check that high priority stuff comes out first, and in FIFO order.
  EXPECT_EQ(queue.Pop(), "learned");
  EXPECT_EQ(queue.Pop(), "refer");
  EXPECT_EQ(queue.Pop(), "mouse");

  // ...and then low priority strings.
  EXPECT_EQ(queue.Pop(), "scan");
  EXPECT_EQ(queue.Pop(), "unit");
  EXPECT_EQ(queue.Pop(), "lmax");
  EXPECT_EQ(queue.Pop(), "omni");
  EXPECT_EQ(queue.Pop(), "startup");
  EXPECT_EQ(queue.Pop(), "omni");

  EXPECT_TRUE(queue.IsEmpty());
}




}  // namespace chrome_browser_net
