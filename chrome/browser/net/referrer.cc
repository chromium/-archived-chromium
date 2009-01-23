// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/referrer.h"

#include "base/logging.h"

namespace chrome_browser_net {

void Referrer::SuggestHost(const std::string& host) {
  // Limit how large our list can get, in case we make mistakes about what
  // hostnames are in sub-resources (example: Some advertisments have a link to
  // the ad agency, and then provide a "surprising" redirect to the advertised
  // entity, which then (mistakenly) appears to be a subresource on the page
  // hosting the ad).
  // TODO(jar): Do experiments to optimize the max count of suggestions.
  static const size_t kMaxSuggestions = 8;

  if (host.empty())
    return;
  if (kMaxSuggestions <= size()) {
    DeleteLeastUseful();
    DCHECK(kMaxSuggestions > size());
  }
  // Add in the new suggestion.
  (*this)[host];
}

void Referrer::DeleteLeastUseful() {
  std::string least_useful_name;
  // We use longs for durations because we will use multiplication on them.
  int64 least_useful_latency = 0;  // Duration in milliseconds.
  int64 least_useful_lifetime = 0;  // Duration in milliseconds.

  const base::Time kNow(base::Time::Now());  // Avoid multiple calls.
  for (HostNameMap::iterator it = this->begin(); it != this->end(); ++it) {
    int64 lifetime = (kNow - it->second.birth_time()).InMilliseconds();
    int64 latency = it->second.latency().InMilliseconds();
    if (!least_useful_name.empty()) {
      if (!latency && !least_useful_latency) {
        // Older name is less useful.
        if (lifetime <= least_useful_lifetime)
          continue;
      } else {
        // Compare the ratios latency/lifetime vs.
        // least_useful_latency/least_useful_lifetime by cross multiplying (to
        // avoid integer division hassles).  Overflow's won't happen until
        // both latency and lifetime pass about 49 days.
        if (latency * least_useful_lifetime >=
            least_useful_latency * lifetime) {
          continue;
        }
      }
    }
    least_useful_name = it->first;
    least_useful_latency = latency;
    least_useful_lifetime = lifetime;
  }
  erase(least_useful_name);
  // Note: there is a small chance that we will discard a least_useful_name
  // that is currently being prefetched because it *was* in this referer list.
  // In that case, when a benefit appears in AccrueValue() below, we are careful
  // to check before accessing the member.
}

void Referrer::AccrueValue(const base::TimeDelta& delta,
                           const std::string host) {
  HostNameMap::iterator it = this->find(host);
  // Be careful that we weren't evicted from this referrer in DeleteLeastUseful.
  if (it != this->end())
    it->second.AccrueValue(delta);
}

}  // namespace chrome_browser_net
