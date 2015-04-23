// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using WebKit::WebCache;

class WebCacheManagerTest : public testing::Test {
 protected:
  typedef WebCacheManager::StatsMap StatsMap;
  typedef WebCacheManager::Allocation Allocation;
  typedef WebCacheManager::AllocationStrategy AllocationStrategy;

  static const int kRendererID;
  static const int kRendererID2;
  static const WebCache::UsageStats kStats;
  static const WebCache::UsageStats kStats2;

  // Thunks to access protected members of WebCacheManager
  static std::map<int, WebCacheManager::RendererInfo>& stats(
        WebCacheManager* h) {
    return h->stats_;
  }

  static void SimulateInactivity(WebCacheManager* h, int renderer_id) {
    stats(h)[renderer_id].access = Time::Now() - TimeDelta::FromMinutes(
        WebCacheManager::kRendererInactiveThresholdMinutes);
    h->FindInactiveRenderers();
  }

  static std::set<int>& active_renderers(WebCacheManager* h) {
    return h->active_renderers_;
  }
  static std::set<int>& inactive_renderers(WebCacheManager* h) {
    return h->inactive_renderers_;
  }
  static void GatherStats(WebCacheManager* h,
                          std::set<int> renderers,
                          WebCache::UsageStats* stats) {
    h->GatherStats(renderers, stats);
  }
  static size_t GetSize(int tactic,
                        const WebCache::UsageStats& stats) {
    return WebCacheManager::GetSize(
        static_cast<WebCacheManager::AllocationTactic>(tactic), stats);
  }
  static bool AttemptTactic(WebCacheManager* h,
                            int active_tactic,
                            const WebCache::UsageStats& active_stats,
                            int inactive_tactic,
                            const WebCache::UsageStats& inactive_stats,
                            std::list< std::pair<int,size_t> >* strategy) {
    return h->AttemptTactic(
        static_cast<WebCacheManager::AllocationTactic>(active_tactic),
        active_stats,
        static_cast<WebCacheManager::AllocationTactic>(inactive_tactic),
        inactive_stats,
        strategy);
  }
  static void AddToStrategy(WebCacheManager* h,
                            std::set<int> renderers,
                            int tactic,
                            size_t extra_bytes_to_allocate,
                            std::list< std::pair<int,size_t> >* strategy) {
    h->AddToStrategy(renderers,
                     static_cast<WebCacheManager::AllocationTactic>(tactic),
                     extra_bytes_to_allocate,
                     strategy);
  }

  enum {
    DIVIDE_EVENLY = WebCacheManager::DIVIDE_EVENLY,
    KEEP_CURRENT_WITH_HEADROOM = WebCacheManager::KEEP_CURRENT_WITH_HEADROOM,
    KEEP_CURRENT = WebCacheManager::KEEP_CURRENT,
    KEEP_LIVE_WITH_HEADROOM = WebCacheManager::KEEP_LIVE_WITH_HEADROOM,
    KEEP_LIVE = WebCacheManager::KEEP_LIVE,
  };

  WebCacheManager* manager() { return &manager_; }

 private:
  WebCacheManager manager_;
  MessageLoop message_loop_;
};

// static
const int WebCacheManagerTest::kRendererID = 146;

// static
const int WebCacheManagerTest::kRendererID2 = 245;

// static
const WebCache::UsageStats WebCacheManagerTest::kStats = {
    0,
    1024 * 1024,
    1024 * 1024,
    256 * 1024,
    512,
  };

// static
const WebCache::UsageStats WebCacheManagerTest::kStats2 = {
    0,
    2 * 1024 * 1024,
    2 * 1024 * 1024,
    2 * 256 * 1024,
    2 * 512,
  };

static bool operator==(const WebCache::UsageStats& lhs,
                       const WebCache::UsageStats& rhs) {
  return !::memcmp(&lhs, &rhs, sizeof(WebCache::UsageStats));
}

TEST_F(WebCacheManagerTest, AddRemoveRendererTest) {
  EXPECT_EQ(0U, active_renderers(manager()).size());
  EXPECT_EQ(0U, inactive_renderers(manager()).size());

  manager()->Add(kRendererID);
  EXPECT_EQ(1U, active_renderers(manager()).count(kRendererID));
  EXPECT_EQ(0U, inactive_renderers(manager()).count(kRendererID));

  manager()->Remove(kRendererID);
  EXPECT_EQ(0U, active_renderers(manager()).size());
  EXPECT_EQ(0U, inactive_renderers(manager()).size());
}

TEST_F(WebCacheManagerTest, ActiveInactiveTest) {
  manager()->Add(kRendererID);

  manager()->ObserveActivity(kRendererID);
  EXPECT_EQ(1U, active_renderers(manager()).count(kRendererID));
  EXPECT_EQ(0U, inactive_renderers(manager()).count(kRendererID));

  SimulateInactivity(manager(), kRendererID);
  EXPECT_EQ(0U, active_renderers(manager()).count(kRendererID));
  EXPECT_EQ(1U, inactive_renderers(manager()).count(kRendererID));

  manager()->ObserveActivity(kRendererID);
  EXPECT_EQ(1U, active_renderers(manager()).count(kRendererID));
  EXPECT_EQ(0U, inactive_renderers(manager()).count(kRendererID));

  manager()->Remove(kRendererID);
}

TEST_F(WebCacheManagerTest, ObserveStatsTest) {
  manager()->Add(kRendererID);

  EXPECT_EQ(1U, stats(manager()).size());

  manager()->ObserveStats(kRendererID, kStats);

  EXPECT_EQ(1U, stats(manager()).size());
  EXPECT_TRUE(kStats == stats(manager())[kRendererID]);

  manager()->Remove(kRendererID);
}

TEST_F(WebCacheManagerTest, SetGlobalSizeLimitTest) {
  size_t limit = manager()->GetDefaultGlobalSizeLimit();
  manager()->SetGlobalSizeLimit(limit);
  EXPECT_EQ(limit, manager()->global_size_limit());

  manager()->SetGlobalSizeLimit(0);
  EXPECT_EQ(0U, manager()->global_size_limit());
}

TEST_F(WebCacheManagerTest, GatherStatsTest) {
  manager()->Add(kRendererID);
  manager()->Add(kRendererID2);

  manager()->ObserveStats(kRendererID, kStats);
  manager()->ObserveStats(kRendererID2, kStats2);

  std::set<int> renderer_set;
  renderer_set.insert(kRendererID);

  WebCache::UsageStats stats;
  GatherStats(manager(), renderer_set, &stats);

  EXPECT_TRUE(kStats == stats);

  renderer_set.insert(kRendererID2);
  GatherStats(manager(), renderer_set, &stats);

  WebCache::UsageStats expected_stats = kStats;
  expected_stats.minDeadCapacity += kStats2.minDeadCapacity;
  expected_stats.maxDeadCapacity += kStats2.maxDeadCapacity;
  expected_stats.capacity += kStats2.capacity;
  expected_stats.liveSize += kStats2.liveSize;
  expected_stats.deadSize += kStats2.deadSize;

  EXPECT_TRUE(expected_stats == stats);

  manager()->Remove(kRendererID);
  manager()->Remove(kRendererID2);
}

TEST_F(WebCacheManagerTest, GetSizeTest) {
  EXPECT_EQ(0U, GetSize(DIVIDE_EVENLY, kStats));
  EXPECT_LT(256 * 1024u + 512, GetSize(KEEP_CURRENT_WITH_HEADROOM, kStats));
  EXPECT_EQ(256 * 1024u + 512, GetSize(KEEP_CURRENT, kStats));
  EXPECT_LT(256 * 1024u, GetSize(KEEP_LIVE_WITH_HEADROOM, kStats));
  EXPECT_EQ(256 * 1024u, GetSize(KEEP_LIVE, kStats));
}

TEST_F(WebCacheManagerTest, AttemptTacticTest) {
  manager()->Add(kRendererID);
  manager()->Add(kRendererID2);

  manager()->ObserveActivity(kRendererID);
  SimulateInactivity(manager(), kRendererID2);

  manager()->ObserveStats(kRendererID, kStats);
  manager()->ObserveStats(kRendererID2, kStats2);

  manager()->SetGlobalSizeLimit(kStats.liveSize + kStats.deadSize +
                        kStats2.liveSize + kStats2.deadSize/2);

  AllocationStrategy strategy;

  EXPECT_FALSE(AttemptTactic(manager(),
                             KEEP_CURRENT,
                             kStats,
                             KEEP_CURRENT,
                             kStats2,
                             &strategy));
  EXPECT_TRUE(strategy.empty());

  EXPECT_TRUE(AttemptTactic(manager(),
                            KEEP_CURRENT,
                            kStats,
                            KEEP_LIVE,
                            kStats2,
                            &strategy));
  EXPECT_EQ(2U, strategy.size());

  AllocationStrategy::iterator iter = strategy.begin();
  while (iter != strategy.end()) {
    if (iter->first == kRendererID)
      EXPECT_LE(kStats.liveSize + kStats.deadSize, iter->second);
    else if (iter->first == kRendererID2)
      EXPECT_LE(kStats2.liveSize, iter->second);
    else
      EXPECT_FALSE("Unexpected entry in strategy");
    ++iter;
  }

  manager()->Remove(kRendererID);
  manager()->Remove(kRendererID2);
}

TEST_F(WebCacheManagerTest, AddToStrategyTest) {
  manager()->Add(kRendererID);
  manager()->Add(kRendererID2);

  std::set<int> renderer_set;
  renderer_set.insert(kRendererID);
  renderer_set.insert(kRendererID2);

  manager()->ObserveStats(kRendererID, kStats);
  manager()->ObserveStats(kRendererID2, kStats2);

  const size_t kExtraBytesToAllocate = 10 * 1024;

  AllocationStrategy strategy;
  AddToStrategy(manager(),
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
      EXPECT_LE(kStats.liveSize + kStats.deadSize, iter->second);
    else if (iter->first == kRendererID2)
      EXPECT_LE(kStats2.liveSize + kStats2.deadSize, iter->second);
    else
      EXPECT_FALSE("Unexpected entry in strategy");
    ++iter;
  }

  size_t expected_total_bytes = kExtraBytesToAllocate +
                                kStats.liveSize + kStats.deadSize +
                                kStats2.liveSize + kStats2.deadSize;

  EXPECT_GE(expected_total_bytes, total_bytes);

  manager()->Remove(kRendererID);
  manager()->Remove(kRendererID2);
}
