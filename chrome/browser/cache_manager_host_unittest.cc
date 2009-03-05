// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "chrome/browser/cache_manager_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/cache_manager.h"

using base::Time;
using base::TimeDelta;

class CacheManagerHostTest : public testing::Test {
 protected:
  typedef CacheManagerHost::StatsMap StatsMap;
  typedef CacheManagerHost::Allocation Allocation;
  typedef CacheManagerHost::AllocationStrategy AllocationStrategy;

  static const int kRendererID;
  static const int kRendererID2;
  static const CacheManager::UsageStats kStats;
  static const CacheManager::UsageStats kStats2;

  // Thunks to access protected members of CacheManagerHost
  static std::map<int, CacheManagerHost::RendererInfo>& stats(
        CacheManagerHost* h) {
    return h->stats_;
  }

  static void SimulateInactivity(CacheManagerHost* h, int renderer_id) {
    stats(h)[renderer_id].access = Time::Now() - TimeDelta::FromMinutes(
        CacheManagerHost::kRendererInactiveThresholdMinutes);
    h->FindInactiveRenderers();
  }

  static std::set<int>& active_renderers(CacheManagerHost* h) {
    return h->active_renderers_;
  }
  static std::set<int>& inactive_renderers(CacheManagerHost* h) {
    return h->inactive_renderers_;
  }
  static void GatherStats(CacheManagerHost* h,
                          std::set<int> renderers,
                          CacheManager::UsageStats* stats) {
    h->GatherStats(renderers, stats);
  }
  static size_t GetSize(int tactic,
                        const CacheManager::UsageStats& stats) {
    return CacheManagerHost::GetSize(
        static_cast<CacheManagerHost::AllocationTactic>(tactic), stats);
  }
  static bool AttemptTactic(CacheManagerHost* h,
                            int active_tactic,
                            const CacheManager::UsageStats& active_stats,
                            int inactive_tactic,
                            const CacheManager::UsageStats& inactive_stats,
                            std::list< std::pair<int,size_t> >* strategy) {
    return h->AttemptTactic(
        static_cast<CacheManagerHost::AllocationTactic>(active_tactic),
        active_stats,
        static_cast<CacheManagerHost::AllocationTactic>(inactive_tactic),
        inactive_stats,
        strategy);
  }
  static void AddToStrategy(CacheManagerHost* h,
                            std::set<int> renderers,
                            int tactic,
                            size_t extra_bytes_to_allocate,
                            std::list< std::pair<int,size_t> >* strategy) {
    h->AddToStrategy(renderers,
                     static_cast<CacheManagerHost::AllocationTactic>(tactic),
                     extra_bytes_to_allocate,
                     strategy);
  }

  enum {
    DIVIDE_EVENLY = CacheManagerHost::DIVIDE_EVENLY,
    KEEP_CURRENT_WITH_HEADROOM = CacheManagerHost::KEEP_CURRENT_WITH_HEADROOM,
    KEEP_CURRENT = CacheManagerHost::KEEP_CURRENT,
    KEEP_LIVE_WITH_HEADROOM = CacheManagerHost::KEEP_LIVE_WITH_HEADROOM,
    KEEP_LIVE = CacheManagerHost::KEEP_LIVE,
  };

 private:
  MessageLoop message_loop_;
};

// static
const int CacheManagerHostTest::kRendererID = 146;

// static
const int CacheManagerHostTest::kRendererID2 = 245;

// static
const CacheManager::UsageStats CacheManagerHostTest::kStats = {
    0,
    1024 * 1024,
    1024 * 1024,
    256 * 1024,
    512,
  };

// static
const CacheManager::UsageStats CacheManagerHostTest::kStats2 = {
    0,
    2 * 1024 * 1024,
    2 * 1024 * 1024,
    2 * 256 * 1024,
    2 * 512,
  };

static bool operator==(const CacheManager::UsageStats& lhs,
                       const CacheManager::UsageStats& rhs) {
  return !::memcmp(&lhs, &rhs, sizeof(CacheManager::UsageStats));
}

TEST_F(CacheManagerHostTest, AddRemoveRendererTest) {
  CacheManagerHost* h = CacheManagerHost::GetInstance();

  EXPECT_EQ(0U, active_renderers(h).size());
  EXPECT_EQ(0U, inactive_renderers(h).size());

  h->Add(kRendererID);
  EXPECT_EQ(1U, active_renderers(h).count(kRendererID));
  EXPECT_EQ(0U, inactive_renderers(h).count(kRendererID));

  h->Remove(kRendererID);
  EXPECT_EQ(0U, active_renderers(h).size());
  EXPECT_EQ(0U, inactive_renderers(h).size());
}

TEST_F(CacheManagerHostTest, ActiveInactiveTest) {
  CacheManagerHost* h = CacheManagerHost::GetInstance();

  h->Add(kRendererID);

  h->ObserveActivity(kRendererID);
  EXPECT_EQ(1U, active_renderers(h).count(kRendererID));
  EXPECT_EQ(0U, inactive_renderers(h).count(kRendererID));

  SimulateInactivity(h, kRendererID);
  EXPECT_EQ(0U, active_renderers(h).count(kRendererID));
  EXPECT_EQ(1U, inactive_renderers(h).count(kRendererID));

  h->ObserveActivity(kRendererID);
  EXPECT_EQ(1U, active_renderers(h).count(kRendererID));
  EXPECT_EQ(0U, inactive_renderers(h).count(kRendererID));

  h->Remove(kRendererID);
}

TEST_F(CacheManagerHostTest, ObserveStatsTest) {
  CacheManagerHost* h = CacheManagerHost::GetInstance();

  h->Add(kRendererID);

  EXPECT_EQ(1U, stats(h).size());

  h->ObserveStats(kRendererID, kStats);

  EXPECT_EQ(1U, stats(h).size());
  EXPECT_TRUE(kStats == stats(h)[kRendererID]);

  h->Remove(kRendererID);
}

TEST_F(CacheManagerHostTest, SetGlobalSizeLimitTest) {
  CacheManagerHost* h = CacheManagerHost::GetInstance();

  size_t limit = h->GetDefaultGlobalSizeLimit();
  h->SetGlobalSizeLimit(limit);
  EXPECT_EQ(limit, h->global_size_limit());

  h->SetGlobalSizeLimit(0);
  EXPECT_EQ(0U, h->global_size_limit());
}

TEST_F(CacheManagerHostTest, GatherStatsTest) {
  CacheManagerHost* h = CacheManagerHost::GetInstance();

  h->Add(kRendererID);
  h->Add(kRendererID2);

  h->ObserveStats(kRendererID, kStats);
  h->ObserveStats(kRendererID2, kStats2);

  std::set<int> renderer_set;
  renderer_set.insert(kRendererID);

  CacheManager::UsageStats stats;
  GatherStats(h, renderer_set, &stats);

  EXPECT_TRUE(kStats == stats);

  renderer_set.insert(kRendererID2);
  GatherStats(h, renderer_set, &stats);

  CacheManager::UsageStats expected_stats = kStats;
  expected_stats.min_dead_capacity += kStats2.min_dead_capacity;
  expected_stats.max_dead_capacity += kStats2.max_dead_capacity;
  expected_stats.capacity += kStats2.capacity;
  expected_stats.live_size += kStats2.live_size;
  expected_stats.dead_size += kStats2.dead_size;

  EXPECT_TRUE(expected_stats == stats);

  h->Remove(kRendererID);
  h->Remove(kRendererID2);
}

TEST_F(CacheManagerHostTest, GetSizeTest) {
  EXPECT_EQ(0U, GetSize(DIVIDE_EVENLY, kStats));
  EXPECT_LT(256 * 1024u + 512, GetSize(KEEP_CURRENT_WITH_HEADROOM, kStats));
  EXPECT_EQ(256 * 1024u + 512, GetSize(KEEP_CURRENT, kStats));
  EXPECT_LT(256 * 1024u, GetSize(KEEP_LIVE_WITH_HEADROOM, kStats));
  EXPECT_EQ(256 * 1024u, GetSize(KEEP_LIVE, kStats));
}

TEST_F(CacheManagerHostTest, AttemptTacticTest) {
  CacheManagerHost* h = CacheManagerHost::GetInstance();

  h->Add(kRendererID);
  h->Add(kRendererID2);

  h->ObserveActivity(kRendererID);
  SimulateInactivity(h, kRendererID2);

  h->ObserveStats(kRendererID, kStats);
  h->ObserveStats(kRendererID2, kStats2);

  h->SetGlobalSizeLimit(kStats.live_size + kStats.dead_size +
                        kStats2.live_size + kStats2.dead_size/2);

  AllocationStrategy strategy;

  EXPECT_FALSE(AttemptTactic(h,
                             KEEP_CURRENT,
                             kStats,
                             KEEP_CURRENT,
                             kStats2,
                             &strategy));
  EXPECT_TRUE(strategy.empty());

  EXPECT_TRUE(AttemptTactic(h,
                            KEEP_CURRENT,
                            kStats,
                            KEEP_LIVE,
                            kStats2,
                            &strategy));
  EXPECT_EQ(2U, strategy.size());

  AllocationStrategy::iterator iter = strategy.begin();
  while (iter != strategy.end()) {
    if (iter->first == kRendererID)
      EXPECT_LE(kStats.live_size + kStats.dead_size, iter->second);
    else if (iter->first == kRendererID2)
      EXPECT_LE(kStats2.live_size, iter->second);
    else
      EXPECT_FALSE("Unexpected entry in strategy");
    ++iter;
  }

  h->Remove(kRendererID);
  h->Remove(kRendererID2);
}

TEST_F(CacheManagerHostTest, AddToStrategyTest) {
  CacheManagerHost* h = CacheManagerHost::GetInstance();

  h->Add(kRendererID);
  h->Add(kRendererID2);

  std::set<int> renderer_set;
  renderer_set.insert(kRendererID);
  renderer_set.insert(kRendererID2);

  h->ObserveStats(kRendererID, kStats);
  h->ObserveStats(kRendererID2, kStats2);

  const size_t kExtraBytesToAllocate = 10 * 1024;

  AllocationStrategy strategy;
  AddToStrategy(h,
                renderer_set,
                KEEP_CURRENT,
                kExtraBytesToAllocate,
                &strategy);

  EXPECT_EQ(2U, strategy.size());

  size_t total_bytes = 0;
  AllocationStrategy::iterator iter = strategy.begin();
  while (iter != strategy.end()) {
    total_bytes += iter->second;

    if (iter->first == kRendererID)
      EXPECT_LE(kStats.live_size + kStats.dead_size, iter->second);
    else if (iter->first == kRendererID2)
      EXPECT_LE(kStats2.live_size + kStats2.dead_size, iter->second);
    else
      EXPECT_FALSE("Unexpected entry in strategy");
    ++iter;
  }

  size_t expected_total_bytes = kExtraBytesToAllocate +
                                kStats.live_size + kStats.dead_size +
                                kStats2.live_size + kStats2.dead_size;

  EXPECT_GE(expected_total_bytes, total_bytes);

  h->Remove(kRendererID);
  h->Remove(kRendererID2);
}

