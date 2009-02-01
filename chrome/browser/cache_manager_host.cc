// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/cache_manager_host.h"

#include "base/sys_info.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"

using base::Time;
using base::TimeDelta;

static const unsigned int kReviseAllocationDelayMS = 200 /* milliseconds */;

// The default size limit of the in-memory cache is 8 MB
static const int kDefaultMemoryCacheSize = 8 * 1024 * 1024;

namespace {

int GetDefaultCacheSize() {
  // Start off with a modest default
  int default_cache_size = kDefaultMemoryCacheSize;

  // Check how much physical memory the OS has
  int mem_size_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  if (mem_size_mb >= 1000)  // If we have a GB of memory, set a larger default.
    default_cache_size *= 4;
  else if (mem_size_mb >= 512)  // With 512 MB, set a slightly larger default.
    default_cache_size *= 2;

  return default_cache_size;
}

}

// static
void CacheManagerHost::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kMemoryCacheSize, GetDefaultCacheSize());
}

// static
CacheManagerHost* CacheManagerHost::GetInstance() {
  return Singleton<CacheManagerHost>::get();
}

CacheManagerHost::CacheManagerHost()
    : global_size_limit_(GetDefaultGlobalSizeLimit()),
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
      revise_allocation_factory_(this) {
}

CacheManagerHost::~CacheManagerHost() {
}

void CacheManagerHost::Add(int renderer_id) {
  DCHECK(inactive_renderers_.count(renderer_id) == 0);

  // It is tempting to make the following DCHECK here, but it fails when a new
  // tab is created as we observe activity from that tab because the
  // RenderProcessHost is recreated and adds itself.
  //
  //   DCHECK(active_renderers_.count(renderer_id) == 0);
  //
  // However, there doesn't seem to be much harm in receiving the calls in this
  // order.

  active_renderers_.insert(renderer_id);

  RendererInfo* stats = &(stats_[renderer_id]);
  memset(stats, 0, sizeof(*stats));
  stats->access = Time::Now();

  // Revise our allocation strategy to account for this new renderer.
  ReviseAllocationStrategyLater();
}

void CacheManagerHost::Remove(int renderer_id) {
  DCHECK(active_renderers_.count(renderer_id) > 0 ||
         inactive_renderers_.count(renderer_id) > 0);

  // Erase all knowledge of this renderer
  active_renderers_.erase(renderer_id);
  inactive_renderers_.erase(renderer_id);
  stats_.erase(renderer_id);

  // Reallocate the resources used by this renderer
  ReviseAllocationStrategyLater();
}

void CacheManagerHost::ObserveActivity(int renderer_id) {
  // Record activity.
  active_renderers_.insert(renderer_id);

  StatsMap::iterator item = stats_.find(renderer_id);
  if (item != stats_.end())
    item->second.access = Time::Now();

  std::set<int>::iterator elmt = inactive_renderers_.find(renderer_id);
  if (elmt != inactive_renderers_.end()) {
    inactive_renderers_.erase(elmt);

    // A renderer that was inactive, just became active.  We should make sure
    // it is given a fair cache allocation, but we defer this for a bit in
    // order to make this function call cheap.
    ReviseAllocationStrategyLater();
  }
}

void CacheManagerHost::ObserveStats(int renderer_id,
                                    const CacheManager::UsageStats& stats) {
  StatsMap::iterator entry = stats_.find(renderer_id);
  if (entry == stats_.end())
    return;  // We might see stats for a renderer that has been destroyed.

  // Record the updated stats.
  entry->second.capacity = stats.capacity;
  entry->second.dead_size = stats.dead_size;
  entry->second.live_size = stats.live_size;
  entry->second.max_dead_capacity = stats.max_dead_capacity;
  entry->second.min_dead_capacity = stats.min_dead_capacity;

  // trigger notification
  CacheManager::UsageStats stats_details(stats);
  // &stats_details is only valid during the notification.
  // See notification_types.h.
  NotificationService::current()->Notify(
      NotificationType::WEB_CACHE_STATS_OBSERVED,
      Source<RenderProcessHost>(RenderProcessHost::FromID(renderer_id)),
      Details<CacheManager::UsageStats>(&stats_details));
}

void CacheManagerHost::SetGlobalSizeLimit(size_t bytes) {
  global_size_limit_ = bytes;
  ReviseAllocationStrategyLater();
}

// static
size_t CacheManagerHost::GetDefaultGlobalSizeLimit() {
  PrefService* perf_service = g_browser_process->local_state();
  if (perf_service)
    return perf_service->GetInteger(prefs::kMemoryCacheSize);

  return GetDefaultCacheSize();
}

void CacheManagerHost::GatherStats(const std::set<int>& renderers,
                                   CacheManager::UsageStats* stats) {
  DCHECK(stats);

  memset(stats, 0, sizeof(CacheManager::UsageStats));

  std::set<int>::const_iterator iter = renderers.begin();
  while (iter != renderers.end()) {
    StatsMap::iterator elmt = stats_.find(*iter);
    if (elmt != stats_.end()) {
      stats->min_dead_capacity += elmt->second.min_dead_capacity;
      stats->max_dead_capacity += elmt->second.max_dead_capacity;
      stats->capacity += elmt->second.capacity;
      stats->live_size += elmt->second.live_size;
      stats->dead_size += elmt->second.dead_size;
    }
    ++iter;
  }
}

// static
size_t CacheManagerHost::GetSize(AllocationTactic tactic,
                                 const CacheManager::UsageStats& stats) {
  switch (tactic) {
  case DIVIDE_EVENLY:
    // We aren't going to reserve any space for existing objects.
    return 0;
  case KEEP_CURRENT_WITH_HEADROOM:
    // We need enough space for our current objects, plus some headroom.
    return 3 * GetSize(KEEP_CURRENT, stats) / 2;
  case KEEP_CURRENT:
    // We need enough space to keep our current objects.
    return stats.live_size + stats.dead_size;
  case KEEP_LIVE_WITH_HEADROOM:
    // We need enough space to keep out live resources, plus some headroom.
    return 3 * GetSize(KEEP_LIVE, stats) / 2;
  case KEEP_LIVE:
    // We need enough space to keep our live resources.
    return stats.live_size;
  default:
    NOTREACHED() << "Unknown cache allocation tactic";
    return 0;
  }
}

bool CacheManagerHost::AttemptTactic(
    AllocationTactic active_tactic,
    const CacheManager::UsageStats& active_stats,
    AllocationTactic inactive_tactic,
    const CacheManager::UsageStats& inactive_stats,
    AllocationStrategy* strategy) {
  DCHECK(strategy);

  size_t active_size = GetSize(active_tactic, active_stats);
  size_t inactive_size = GetSize(inactive_tactic, inactive_stats);

  // Give up if we don't have enough space to use this tactic.
  if (global_size_limit_ < active_size + inactive_size)
    return false;

  // Compute the unreserved space available.
  size_t total_extra = global_size_limit_ - (active_size + inactive_size);

  // The plan for the extra space is to divide it evenly amoung the active
  // renderers.
  size_t shares = active_renderers_.size();

  // The inactive renderers get one share of the extra memory to be divided
  // among themselves.
  size_t inactive_extra = 0;
  if (inactive_renderers_.size() > 0) {
    ++shares;
    inactive_extra = total_extra / shares;
  }

  // The remaining memory is allocated to the active renderers.
  size_t active_extra = total_extra - inactive_extra;

  // Actually compute the allocations for each renderer.
  AddToStrategy(active_renderers_, active_tactic, active_extra, strategy);
  AddToStrategy(inactive_renderers_, inactive_tactic, inactive_extra, strategy);

  // We succeeded in computing an allocation strategy.
  return true;
}

void CacheManagerHost::AddToStrategy(std::set<int> renderers,
                                     AllocationTactic tactic,
                                     size_t extra_bytes_to_allocate,
                                     AllocationStrategy* strategy) {
  DCHECK(strategy);

  // Nothing to do if there are no renderers.  It is common for there to be no
  // inactive renderers if there is a single active tab.
  if (renderers.size() == 0)
    return;

  // Divide the extra memory evenly among the renderers.
  size_t extra_each = extra_bytes_to_allocate / renderers.size();

  std::set<int>::const_iterator iter = renderers.begin();
  while (iter != renderers.end()) {
    size_t cache_size = extra_each;

    // Add in the space required to implement |tactic|.
    StatsMap::iterator elmt = stats_.find(*iter);
    if (elmt != stats_.end())
      cache_size += GetSize(tactic, elmt->second);

    // Record the allocation in our strategy.
    strategy->push_back(Allocation(*iter, cache_size));
    ++iter;
  }
}

void CacheManagerHost::EnactStrategy(const AllocationStrategy& strategy) {
  // Inform each render process of its cache allocation.
  AllocationStrategy::const_iterator allocation = strategy.begin();
  while (allocation != strategy.end()) {
    RenderProcessHost* host = RenderProcessHost::FromID(allocation->first);
    if (host) {
      // This is the capacity this renderer has been allocated.
      size_t capacity = allocation->second;

      // We don't reserve any space for dead objects in the cache.  Instead, we
      // prefer to keep live objects around.  There is probably some performance
      // tuning to be done here.
      size_t min_dead_capacity = 0;

      // We allow the dead objects to consume all of the cache, if the renderer
      // so desires.  If we wanted this memory, we would have set the total
      // capacity lower.
      size_t max_dead_capacity = capacity;

      host->Send(new ViewMsg_SetCacheCapacities(min_dead_capacity,
                                                max_dead_capacity,
                                                capacity));
    }
    ++allocation;
  }
}

void CacheManagerHost::ReviseAllocationStrategy() {
  DCHECK(stats_.size() <=
      active_renderers_.size() + inactive_renderers_.size());

  // Check if renderers have gone inactive.
  FindInactiveRenderers();

  // Gather statistics
  CacheManager::UsageStats active;
  CacheManager::UsageStats inactive;
  GatherStats(active_renderers_, &active);
  GatherStats(inactive_renderers_, &inactive);

  // Compute an allocation strategy.
  //
  // We attempt various tactics in order of preference.  Our first preference
  // is not to evict any objects.  If we don't have enough resources, we'll
  // first try to evict dead data only.  If that fails, we'll just divide the
  // resources we have evenly.
  //
  // We always try to give the active renderers some head room in their
  // allocations so they can take memory away from an inactive renderer with
  // a large cache allocation.
  //
  // Notice the early exit will prevent attempting less desirable tactics once
  // we've found a workable strategy.
  AllocationStrategy strategy;
  if (  // Ideally, we'd like to give the active renderers some headroom and
        // keep all our current objects.
      AttemptTactic(KEEP_CURRENT_WITH_HEADROOM, active,
                    KEEP_CURRENT, inactive, &strategy) ||
      // If we can't have that, then we first try to evict the dead objects in
      // the caches of inactive renderers.
      AttemptTactic(KEEP_CURRENT_WITH_HEADROOM, active,
                    KEEP_LIVE, inactive, &strategy) ||
      // Next, we try to keep the live objects in the active renders (with some
      // room for new objects) and give whatever is left to the inactive
      // renderers.
      AttemptTactic(KEEP_LIVE_WITH_HEADROOM, active,
                    DIVIDE_EVENLY, inactive, &strategy) ||
      // If we've gotten this far, then we are very tight on memory.  Let's try
      // to at least keep around the live objects for the active renderers.
      AttemptTactic(KEEP_LIVE, active, DIVIDE_EVENLY, inactive, &strategy) ||
      // We're basically out of memory.  The best we can do is just divide up
      // what we have and soldier on.
      AttemptTactic(DIVIDE_EVENLY, active, DIVIDE_EVENLY, inactive, &strategy)) {
    // Having found a workable strategy, we enact it.
    EnactStrategy(strategy);
  } else {
    // DIVIDE_EVENLY / DIVIDE_EVENLY should always succeed.
    NOTREACHED() << "Unable to find a cache allocation";
  }
}

void CacheManagerHost::ReviseAllocationStrategyLater() {
  // Ask to be called back in a few milliseconds to actually recompute our
  // allocation.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      revise_allocation_factory_.NewRunnableMethod(
          &CacheManagerHost::ReviseAllocationStrategy),
      kReviseAllocationDelayMS);
}

void CacheManagerHost::FindInactiveRenderers() {
  std::set<int>::const_iterator iter = active_renderers_.begin();
  while (iter != active_renderers_.end()) {
    StatsMap::iterator elmt = stats_.find(*iter);
    DCHECK(elmt != stats_.end());
    TimeDelta idle = Time::Now() - elmt->second.access;
    if (idle >= TimeDelta::FromMinutes(kRendererInactiveThresholdMinutes)) {
      // Moved to inactive status.  This invalidates our iterator.
      inactive_renderers_.insert(*iter);
      active_renderers_.erase(*iter);
      iter = active_renderers_.begin();
      continue;
    }
    ++iter;
  }
}

