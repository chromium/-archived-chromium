// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
// Instead of providing accessors, we make all members of Cache public.
// This will make it easier to track WebKit changes to the Cache class.
#define private public
#include "Cache.h"
#undef private
MSVC_POP_WARNING();

#undef LOG
#include "base/logging.h"
#include "webkit/glue/cache_manager.h"

namespace {

// A helper method for coverting a WebCore::Cache::TypeStatistic to a
// CacheManager::ResourceTypeStat.
CacheManager::ResourceTypeStat TypeStatisticToResourceTypeStat(
    const WebCore::Cache::TypeStatistic& in_stat) {
  CacheManager::ResourceTypeStat stat;
  stat.count = static_cast<size_t>(in_stat.count);
  stat.size = static_cast<size_t>(in_stat.size);
  stat.live_size = static_cast<size_t>(in_stat.liveSize);
  stat.decoded_size = static_cast<size_t>(in_stat.decodedSize);
  return stat;
}

}  // namespace

// ----------------------------------------------------------------------------
// CacheManager implementation

CacheManager::CacheManager() {
}

CacheManager::~CacheManager() {
}

// static
void CacheManager::GetUsageStats(UsageStats* result) {
  DCHECK(result);

  WebCore::Cache* cache = WebCore::cache();

  if (cache) {
    result->min_dead_capacity = cache->m_minDeadCapacity;
    result->max_dead_capacity = cache->m_maxDeadCapacity;
    result->capacity = cache->m_capacity;
    result->live_size = cache->m_liveSize;
    result->dead_size = cache->m_deadSize;
  } else {
    memset(result, 0, sizeof(UsageStats));
  }
}

// static
void CacheManager::SetCapacities(size_t min_dead_capacity,
                                 size_t max_dead_capacity,
                                 size_t capacity) {
  WebCore::Cache* cache = WebCore::cache();

  if (cache) {
    cache->setCapacities(static_cast<unsigned int>(min_dead_capacity),
                         static_cast<unsigned int>(max_dead_capacity),
                         static_cast<unsigned int>(capacity));
  }
}

// static
void CacheManager::GetResourceTypeStats(
    CacheManager::ResourceTypeStats* result) {
  WebCore::Cache* cache = WebCore::cache();
  if (cache) {
    WebCore::Cache::Statistics in_stats = cache->getStatistics();
    result->images = TypeStatisticToResourceTypeStat(in_stats.images);
    result->css_stylesheets = TypeStatisticToResourceTypeStat(
        in_stats.cssStyleSheets);
    result->scripts = TypeStatisticToResourceTypeStat(in_stats.scripts);
    result->xsl_stylesheets = TypeStatisticToResourceTypeStat(
        in_stats.xslStyleSheets);
    result->fonts = TypeStatisticToResourceTypeStat(in_stats.fonts);
  } else {
    memset(result, 0, sizeof(CacheManager::ResourceTypeStats));
  }
}

