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
