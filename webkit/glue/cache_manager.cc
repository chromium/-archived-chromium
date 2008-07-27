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

#include "config.h"

#pragma warning(push, 0)
// Instead of providing accessors, we make all members of Cache public.
// This will make it easier to track WebKit changes to the Cache class.
#define private public
#include "Cache.h"
#undef private
#pragma warning(pop)

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
