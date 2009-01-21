// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the browser side of the cache manager, it tracks the activity of the
// render processes and allocates available memory cache resources.

#ifndef CHROME_BROWSER_CACHE_MANAGER_HOST_H__
#define CHROME_BROWSER_CACHE_MANAGER_HOST_H__

#include <map>
#include <list>
#include <set>

#include "base/basictypes.h"
#include "base/shared_memory.h"
#include "base/singleton.h"
#include "base/task.h"
#include "base/time.h"
#include "webkit/glue/cache_manager.h"

class PrefService;

class CacheManagerHost {
  // Unit tests are our friends.
  friend class CacheManagerHostTest;

 public:
  static void RegisterPrefs(PrefService* prefs);

  // Gets the singleton CacheManagerHost object.  The first time this method
  // is called, a CacheManagerHost object is constructed and returned.
  // Subsequent calls will return the same object.
  static CacheManagerHost* GetInstance();

  // When a render process is created, it registers itself with the cache
  // manager host, causing the renderer to be allocated cache resources.
  void Add(int renderer_id);

  // When a render process ends, it removes itself from the cache manager host,
  // freeing the manager to assign its cache resources to other renderers.
  void Remove(int renderer_id);

  // The cache manager assigns more cache resources to active renderer.  When a
  // renderer is active, it should inform the cache manager to receive more
  // cache resources.
  //
  // When a renderer moves from being inactive to being active, the cache
  // manager may decide to adjust its resource allocation, but it will delay
  // the recalculation, allowing ObserveActivity to return quickly.
  void ObserveActivity(int renderer_id);

  // Periodically, renderers should inform the cache manager of their current
  // statistics.  The more up-to-date the cache manager's statistics, the
  // better it can allocate cache resources.
  void ObserveStats(int renderer_id, const CacheManager::UsageStats& stats);

  // The global limit on the number of bytes in all the in-memory caches.
  size_t global_size_limit() const { return global_size_limit_; }

  // Sets the global size limit, forcing a recalculation of cache allocations.
  void SetGlobalSizeLimit(size_t bytes);

  // Gets the default global size limit.  This interrogates system metrics to
  // tune the default size to the current system.
  static size_t GetDefaultGlobalSizeLimit();

 protected:
  // The amount of idle time before we consider a tab to be "inactive"
  static const int kRendererInactiveThresholdMinutes = 5;

  // Keep track of some renderer information.
  struct RendererInfo : CacheManager::UsageStats {
    // The access time for this renderer.
    base::Time access;
  };

  typedef std::map<int, RendererInfo> StatsMap;

  // An allocation is the number of bytes a specific renderer should use for
  // its cache.
  typedef std::pair<int,size_t> Allocation;

  // An allocation strategy is a list of allocations specifying the resources
  // each renderer is permitted to consume for its cache.
  typedef std::list<Allocation> AllocationStrategy;

  // This class is a singleton.  Do not instantiate directly.
  CacheManagerHost();
  friend DefaultSingletonTraits<CacheManagerHost>;

  ~CacheManagerHost();

  // Recomputes the allocation of cache resources among the renderers.  Also
  // informs the renderers of their new allocation.
  void ReviseAllocationStrategy();

  // Schedules a call to ReviseAllocationStrategy after a short delay.
  void ReviseAllocationStrategyLater();

  // The various tactics used as part of an allocation strategy.  To decide
  // how many resources a given renderer should be allocated, we consider its
  // usage statistics.  Each tactic specifies the function that maps usage
  // statistics to resource allocations.
  //
  // Determining a resource allocation strategy amounts to picking a tactic
  // for each renderer and checking that the total memory required fits within
  // our |global_size_limit_|.
  enum AllocationTactic {
    // Ignore cache statistics and divide resources equally among the given
    // set of caches.
    DIVIDE_EVENLY,

    // Allow each renderer to keep its current set of cached resources, with
    // some extra allocation to store new objects.
    KEEP_CURRENT_WITH_HEADROOM,

    // Allow each renderer to keep its current set of cached resources.
    KEEP_CURRENT,

    // Allow each renderer to keep cache resources it believs are currently
    // being used, with some extra allocation to store new objects.
    KEEP_LIVE_WITH_HEADROOM,

    // Allow each renderer to keep cache resources it believes are currently
    // being used, but instruct the renderer to discard all other data.
    KEEP_LIVE,
  };

  // Helper functions for devising an allocation strategy

  // Add up all the stats from the given set of renderers and place the result
  // in |stats|.
  void GatherStats(const std::set<int>& renderers,
                   CacheManager::UsageStats* stats);

  // Get the amount of memory that would be required to implement |tactic|
  // using the specified allocation tactic.  This function defines the
  // semantics for each of the tactics.
  static size_t CacheManagerHost::GetSize(AllocationTactic tactic,
                                          const CacheManager::UsageStats& stats);

  // Attempt to use the specified tactics to compute an allocation strategy
  // and place the result in |strategy|.  |active_stats| and |inactive_stats|
  // are the aggregate statistics for |active_renderers_| and
  // |inactive_renderers_|, respectively.
  //
  // Returns |true| on success and |false| on failure.  Does not modify
  // |strategy| on failure.
  bool AttemptTactic(AllocationTactic active_tactic,
                     const CacheManager::UsageStats& active_stats,
                     AllocationTactic inactive_tactic,
                     const CacheManager::UsageStats& inactive_stats,
                     AllocationStrategy* strategy);

  // For each renderer in |renderers|, computes its allocation according to
  // |tactic| and add the result to |strategy|.  Any |extra_bytes_to_allocate|
  // is divided evenly among the renderers.
  void AddToStrategy(std::set<int> renderers,
                     AllocationTactic tactic,
                     size_t extra_bytes_to_allocate,
                     AllocationStrategy* strategy);

  // Enact an allocation strategy by informing the renderers of their
  // allocations according to |strategy|.
  void EnactStrategy(const AllocationStrategy& strategy);

  // Check to see if any active renderers have fallen inactive.
  void FindInactiveRenderers();

  // The global size limit for all in-memory caches.
  size_t global_size_limit_;

  // Maps every renderer_id our most recent copy of its statistics.
  StatsMap stats_;

  // Every renderer we think is still around is in one of these two sets.
  //
  // Active renderers are those renderers that have been active more recently
  // than they have been inactive.
  std::set<int> active_renderers_;
  // Inactive renderers are those renderers that have been inactive more
  // recently than they have been active.
  std::set<int> inactive_renderers_;

  ScopedRunnableMethodFactory<CacheManagerHost> revise_allocation_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(CacheManagerHost);
};

#endif  // CHROME_BROWSER_CACHE_MANAGER_HOST_H__

