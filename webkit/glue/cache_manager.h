// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_CACHE_MANAGER_H__
#define WEBKIT_GLUE_CACHE_MANAGER_H__

#include <stddef.h>

class CacheManager {
 public:
  struct UsageStats {
   public:
    // Capacities.
    size_t min_dead_capacity;
    size_t max_dead_capacity;
    size_t capacity;
    // Utilization.
    size_t live_size;
    size_t dead_size;
  };

  // A struct mirroring WebCore::Cache::TypeStatistic that we can send to the
  // browser process.
  struct ResourceTypeStat {
    size_t count;
    size_t size;
    size_t live_size;
    size_t decoded_size;
    ResourceTypeStat()
        : count(0), size(0), live_size(0), decoded_size(0) {}
  };

  // A struct mirroring WebCore::Cache::Statistics that we can send to the
  // browser process.
  struct ResourceTypeStats {
    ResourceTypeStat images;
    ResourceTypeStat css_stylesheets;
    ResourceTypeStat scripts;
    ResourceTypeStat xsl_stylesheets;
    ResourceTypeStat fonts;
  };

  // Gets the usage statistics from the WebCore cache
  static void GetUsageStats(UsageStats* result);

  // Sets the capacities of the WebCore cache, evicting objects as necessary
  static void SetCapacities(size_t min_dead_capacity,
                            size_t max_dead_capacity,
                            size_t capacity);

  // Get usage stats about the WebCore cache.
  static void GetResourceTypeStats(ResourceTypeStats* result);

 private:
  // This class only has static methods.  It can't be instantiated
  CacheManager();
  ~CacheManager();

  DISALLOW_EVIL_CONSTRUCTORS(CacheManager);
};

#endif  // WEBKIT_GLUE_CACHE_MANAGER_H__
