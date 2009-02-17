// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multi-threaded tests of DnsMaster and DnsPrefetch slave functionality.

#include <time.h>
#include <ws2tcpip.h>
#include <Wspiapi.h>  // Needed for win2k compatibility

#include <algorithm>
#include <map>
#include <sstream>
#include <string>

#include "base/platform_thread.h"
#include "base/spin_wait.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/net/dns_host_info.h"
#include "chrome/browser/net/dns_slave.h"
#include "net/base/winsock_init.h"
#include "testing/gtest/include/gtest/gtest.h"


using base::Time;
using base::TimeDelta;

namespace {

class DnsMasterTest : public testing::Test {
};

typedef chrome_browser_net::DnsMaster DnsMaster;
typedef chrome_browser_net::DnsPrefetcherInit DnsPrefetcherInit;
typedef chrome_browser_net::DnsHostInfo DnsHostInfo;
typedef chrome_browser_net::NameList NameList;


//------------------------------------------------------------------------------
// Provide network function stubs to run tests offline (and avoid the variance
// of real DNS lookups.
//------------------------------------------------------------------------------

static void __stdcall fake_free_addr_info(struct addrinfo* ai) {
  // Kill off the dummy results.
  EXPECT_TRUE(NULL != ai);
  delete ai;
}

static int __stdcall fake_get_addr_info(const char* nodename,
                                        const char* servname,
                                        const struct addrinfo* hints,
                                        struct addrinfo** result) {
  static Lock lock;
  int duration;
  bool was_found;
  std::string hostname(nodename);
  // Dummy up *some* return results to pass along.
  *result = new addrinfo;
  EXPECT_TRUE(NULL != *result);
  {
    AutoLock autolock(lock);

    static bool initialized = false;
    typedef std::map<std::string, int> Latency;
    static Latency latency;
    static std::map<std::string, bool> found;
    if (!initialized) {
      initialized = true;
      // List all known hostnames
      latency["www.google.com"] = 50;
      latency["gmail.google.com.com"] = 70;
      latency["mail.google.com"] = 44;
      latency["gmail.com"] = 63;

      for (Latency::iterator it = latency.begin(); latency.end() != it; it++) {
        found[it->first] = true;
      }
    }  // End static initialization

    was_found = found[hostname];

    if (latency.end() != latency.find(hostname)) {
      duration = latency[hostname];
    } else {
      duration = 500;
    }
    // Change latency to simulate cache warming (next latency will be short).
    latency[hostname] = 1;
  }  // Release lock.

  PlatformThread::Sleep(duration);

  return was_found ? 0 : WSAHOST_NOT_FOUND;
}

static void SetupNetworkInfrastructure() {
  bool kUseFakeNetwork = true;
  if (kUseFakeNetwork)
    chrome_browser_net::SetAddrinfoCallbacks(fake_get_addr_info,
                                                 fake_free_addr_info);
}

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
    char* port = "80";  // I may need to get the real port
    struct addrinfo* result = NULL;
    Time start = Time::Now();

    // Use the same underlying methods as dns_prefetch_slave does
    chrome_browser_net::get_getaddrinfo()(hostname.c_str(), port,
                                              NULL, &result);

    TimeDelta duration = Time::Now() - start;

    if (result) {
      chrome_browser_net::get_freeaddrinfo()(result);
      result = NULL;
    }

    return duration;
}

//------------------------------------------------------------------------------

// First test to be sure the OS is caching lookups, which is the whole premise
// of DNS prefetching.
TEST(DnsMasterTest, OsCachesLookupsTest) {
  SetupNetworkInfrastructure();
  net::EnsureWinsockInit();

  // To avoid flaky nature of a timed test, we'll run an outer loop until we get
  // 90% of the inner loop tests to pass.
  // Originally we just did one set of 5 tests and demand 100%, but that proved
  // flakey.  With this set-looping approach, we always pass in one set (when it
  // used to pass). If we don't get that first set to pass, then we allow an
  // average of one failure per two major sets. If the test runs too long, then
  // there probably is a real problem, and the test harness will terminate us
  // with a failure.
  int pass_count(0);
  int fail_count(0);
  do {
    for (int i = 0; i < 5; i++) {
      std::string badname;
      badname = GetNonexistantDomain();
      TimeDelta duration = BlockingDnsLookup(badname);
      TimeDelta cached_duration = BlockingDnsLookup(badname);
      if (duration > cached_duration)
        pass_count++;
      else
        fail_count++;
    }
  } while (fail_count * 9 > pass_count);
}

TEST(DnsMasterTest, StartupShutdownTest) {
  DnsMaster testing_master(TimeDelta::FromMilliseconds(5000));

  // With no threads, we should have no problem doing a shutdown.
  EXPECT_TRUE(testing_master.ShutdownSlaves());
}

TEST(DnsMasterTest, BenefitLookupTest) {
  SetupNetworkInfrastructure();
  net::EnsureWinsockInit();
  DnsPrefetcherInit dns_init(NULL);  // Creates global service .
  DnsMaster testing_master(TimeDelta::FromMilliseconds(5000));

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

  // First only cause a minimal set of threads to start up.
  // Currently we actually start 4 threads when we get called with an array
  testing_master.ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  // Wait for some resoultion for each google.
  SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(0 <=
      testing_master.GetResolutionDuration(goog).InMilliseconds());
  SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(0 <=
      testing_master.GetResolutionDuration(goog2).InMilliseconds());
  SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(0 <=
      testing_master.GetResolutionDuration(goog3).InMilliseconds());
  SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(0 <=
      testing_master.GetResolutionDuration(goog4).InMilliseconds());

  EXPECT_EQ(std::min(names.size(),
                     4u /* chrome_browser_net::DnsMaster::kSlaveCountMin */ ),
            testing_master.running_slave_count());

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

  // Ensure a clean shutdown.
  EXPECT_TRUE(testing_master.ShutdownSlaves());
}

TEST(DnsMasterTest, DISABLED_SingleSlaveLookupTest) {
  SetupNetworkInfrastructure();
  net::EnsureWinsockInit();
  DnsPrefetcherInit dns_init(NULL);  // Creates global service.
  DnsMaster testing_master(TimeDelta::FromMilliseconds(5000));

  std::string goog("www.google.com"),
    goog2("gmail.google.com.com"),
    goog3("mail.google.com"),
    goog4("gmail.com");
  std::string bad1(GetNonexistantDomain()),
    bad2(GetNonexistantDomain());

  // Warm up local OS cache.
  BlockingDnsLookup(goog);

  NameList names;
  names.insert(names.end(), goog);
  names.insert(names.end(), bad1);
  names.insert(names.end(), bad2);

  // First only cause a single thread to start up
  testing_master.ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  // Wait for some resoultion for google.
  SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(0 <=
      testing_master.GetResolutionDuration(goog).InMilliseconds());

  EXPECT_TRUE(testing_master.WasFound(goog));
  EXPECT_FALSE(testing_master.WasFound(bad1));
  EXPECT_FALSE(testing_master.WasFound(bad2));
  // Verify the reason it is not found is that it is still being proceessed.
  // Negative time mean no resolution yet.
  EXPECT_GT(0, testing_master.GetResolutionDuration(bad2).InMilliseconds());

  // Spin long enough that we *do* find the resolution of bad2.
  SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(0 <=
      testing_master.GetResolutionDuration(bad2).InMilliseconds());

  // Verify both fictitious names are resolved by now.
  // Typical random name takes about 20-30 ms
  EXPECT_LT(0, testing_master.GetResolutionDuration(bad1).InMilliseconds());
  EXPECT_LT(0, testing_master.GetResolutionDuration(bad2).InMilliseconds());
  EXPECT_FALSE(testing_master.WasFound(bad1));
  EXPECT_FALSE(testing_master.WasFound(bad2));

  EXPECT_EQ(1U, testing_master.running_slave_count());

  // With just one thread (doing nothing now), ensure a clean shutdown.
  EXPECT_TRUE(testing_master.ShutdownSlaves());
}

TEST(DnsMasterTest, DISABLED_MultiThreadedLookupTest) {
  SetupNetworkInfrastructure();
  net::EnsureWinsockInit();
  DnsMaster testing_master(TimeDelta::FromSeconds(30));
  DnsPrefetcherInit dns_init(NULL);

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

  // Get all 8 threads running by calling many times before queue is handled.
  for (int i = 0; i < 10; i++) {
    testing_master.ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);
  }

  Sleep(10);  // Allow time for async DNS to get answers.

  EXPECT_TRUE(testing_master.WasFound(goog));
  EXPECT_TRUE(testing_master.WasFound(goog3));
  EXPECT_TRUE(testing_master.WasFound(goog2));
  EXPECT_TRUE(testing_master.WasFound(goog4));
  EXPECT_FALSE(testing_master.WasFound(bad1));
  EXPECT_FALSE(testing_master.WasFound(bad2));

  EXPECT_EQ(8U, testing_master.running_slave_count());

  EXPECT_TRUE(testing_master.ShutdownSlaves());
}

TEST(DnsMasterTest, DISABLED_MultiThreadedSpeedupTest) {
  SetupNetworkInfrastructure();
  net::EnsureWinsockInit();
  DnsMaster testing_master(TimeDelta::FromSeconds(30));
  DnsPrefetcherInit dns_init(NULL);

  std::string goog("www.google.com"),
    goog2("gmail.google.com.com"),
    goog3("mail.google.com"),
    goog4("gmail.com");
  std::string bad1(GetNonexistantDomain()),
    bad2(GetNonexistantDomain()),
    bad3(GetNonexistantDomain()),
    bad4(GetNonexistantDomain());

  NameList names;
  names.insert(names.end(), goog);
  names.insert(names.end(), bad1);
  names.insert(names.end(), bad2);
  names.insert(names.end(), goog3);
  names.insert(names.end(), goog2);
  names.insert(names.end(), bad3);
  names.insert(names.end(), bad4);
  names.insert(names.end(), goog4);

  // First cause a lookup using a single thread.
  testing_master.ResolveList(names, DnsHostInfo::PAGE_SCAN_MOTIVATED);

  // Wait for some resoultion for google.
  SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(0 <=
      testing_master.GetResolutionDuration(goog).InMilliseconds());

  EXPECT_TRUE(testing_master.WasFound(goog));
  EXPECT_FALSE(testing_master.WasFound(bad1));
  EXPECT_FALSE(testing_master.WasFound(bad2));
  // ...and due to delay in geting resolution of bad names, the single slave
  // thread won't have time to finish the list.
  EXPECT_FALSE(testing_master.WasFound(goog3));
  EXPECT_FALSE(testing_master.WasFound(goog2));
  EXPECT_FALSE(testing_master.WasFound(goog4));

  EXPECT_EQ(1U, testing_master.running_slave_count());

  // Get all 8 threads running by calling many times before queue is handled.
  names.clear();
  for (int i = 0; i < 10; i++)
    testing_master.Resolve(GetNonexistantDomain(),
                           DnsHostInfo::PAGE_SCAN_MOTIVATED);

  // Wait long enough for all the goog's to be resolved.
  // They should all take about the same time, and run in parallel.
  SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(0 <=
      testing_master.GetResolutionDuration(goog2).InMilliseconds());
  SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(0 <=
      testing_master.GetResolutionDuration(goog3).InMilliseconds());
  SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(0 <=
      testing_master.GetResolutionDuration(goog4).InMilliseconds());

  EXPECT_TRUE(testing_master.WasFound(goog3));
  EXPECT_TRUE(testing_master.WasFound(goog2));
  EXPECT_TRUE(testing_master.WasFound(goog4));

  EXPECT_FALSE(testing_master.WasFound(bad1));
  EXPECT_FALSE(testing_master.WasFound(bad2));  // Perhaps not even decided.

  // Queue durations should be distinct from when 1 slave was working.
  EXPECT_GT(testing_master.GetQueueDuration(goog3).InMilliseconds(),
              testing_master.GetQueueDuration(goog).InMilliseconds());
  EXPECT_GT(testing_master.GetQueueDuration(goog4).InMilliseconds(),
              testing_master.GetQueueDuration(goog).InMilliseconds());

  // Give bad names a chance to be determined as unresolved.
  SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(0 <=
      testing_master.GetResolutionDuration(bad1).InMilliseconds());
  SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(0 <=
      testing_master.GetResolutionDuration(bad2).InMilliseconds());


  // Well known names should resolve faster than bad names.
  EXPECT_GE(testing_master.GetResolutionDuration(bad1).InMilliseconds(),
              testing_master.GetResolutionDuration(goog).InMilliseconds());

  EXPECT_GE(testing_master.GetResolutionDuration(bad2).InMilliseconds(),
              testing_master.GetResolutionDuration(goog4).InMilliseconds());

  EXPECT_EQ(8U, testing_master.running_slave_count());

  EXPECT_TRUE(testing_master.ShutdownSlaves());
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
TEST(DnsMasterTest, ReferrerSerializationNilTest) {
  DnsMaster master(TimeDelta::FromSeconds(30));
  ListValue referral_list;
  master.SerializeReferrers(&referral_list);
  EXPECT_EQ(0, referral_list.GetSize());
  EXPECT_EQ(kLatencyNotFound, GetLatencyFromSerialization("a.com", "b.com",
                                                          referral_list));
}

// Make sure that when a serialization list includes a value, that it can be
// deserialized into the database, and can be extracted back out via
// serialization without being changed.
TEST(DnsMasterTest, ReferrerSerializationSingleReferrerTest) {
  DnsMaster master(TimeDelta::FromSeconds(30));
  std::string motivation_hostname = "www.google.com";
  std::string subresource_hostname = "icons.google.com";
  const int kLatency = 3;
  ListValue referral_list;

  AddToSerializedList(motivation_hostname, subresource_hostname, kLatency,
                      &referral_list);

  master.DeserializeReferrers(referral_list);

  ListValue recovered_referral_list;
  master.SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(1, recovered_referral_list.GetSize());
  EXPECT_EQ(kLatency, GetLatencyFromSerialization(motivation_hostname,
                                                  subresource_hostname,
                                                  recovered_referral_list));
}

// Make sure the Trim() functionality works as expected.
TEST(DnsMasterTest, ReferrerSerializationTrimTest) {
  DnsMaster master(TimeDelta::FromSeconds(30));
  std::string motivation_hostname = "www.google.com";
  std::string icon_subresource_hostname = "icons.google.com";
  std::string img_subresource_hostname = "img.google.com";
  ListValue referral_list;

  AddToSerializedList(motivation_hostname, icon_subresource_hostname, 10,
                      &referral_list);
  AddToSerializedList(motivation_hostname, img_subresource_hostname, 3,
                      &referral_list);

  master.DeserializeReferrers(referral_list);

  ListValue recovered_referral_list;
  master.SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(1, recovered_referral_list.GetSize());
  EXPECT_EQ(10, GetLatencyFromSerialization(motivation_hostname,
                                            icon_subresource_hostname,
                                            recovered_referral_list));
  EXPECT_EQ(3, GetLatencyFromSerialization(motivation_hostname,
                                            img_subresource_hostname,
                                            recovered_referral_list));

  // Each time we Trim, the latency figures should reduce by a factor of two,
  // until they both are 0, an then a trim will delete the whole entry.
  master.TrimReferrers();
  master.SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(1, recovered_referral_list.GetSize());
  EXPECT_EQ(5, GetLatencyFromSerialization(motivation_hostname,
                                            icon_subresource_hostname,
                                            recovered_referral_list));
  EXPECT_EQ(1, GetLatencyFromSerialization(motivation_hostname,
                                            img_subresource_hostname,
                                            recovered_referral_list));

  master.TrimReferrers();
  master.SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(1, recovered_referral_list.GetSize());
  EXPECT_EQ(2, GetLatencyFromSerialization(motivation_hostname,
                                            icon_subresource_hostname,
                                            recovered_referral_list));
  EXPECT_EQ(0, GetLatencyFromSerialization(motivation_hostname,
                                            img_subresource_hostname,
                                            recovered_referral_list));

  master.TrimReferrers();
  master.SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(1, recovered_referral_list.GetSize());
  EXPECT_EQ(1, GetLatencyFromSerialization(motivation_hostname,
                                           icon_subresource_hostname,
                                           recovered_referral_list));
  EXPECT_EQ(0, GetLatencyFromSerialization(motivation_hostname,
                                           img_subresource_hostname,
                                           recovered_referral_list));

  master.TrimReferrers();
  master.SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(0, recovered_referral_list.GetSize());
  EXPECT_EQ(kLatencyNotFound,
            GetLatencyFromSerialization(motivation_hostname,
                                        icon_subresource_hostname,
                                        recovered_referral_list));
  EXPECT_EQ(kLatencyNotFound,
            GetLatencyFromSerialization(motivation_hostname,
                                        img_subresource_hostname,
                                        recovered_referral_list));
}


}  // namespace

