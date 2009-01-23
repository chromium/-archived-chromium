// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class helps to remember what domains may be needed to be resolved when a
// navigation takes place to a given URL.  This information is gathered when a
// navigation resolution was not foreseen by identifying the referrer field that
// induced the navigation.  When future navigations take place to known referrer
// sites, then we automatically pre-resolve the expected set of useful domains.

// All access to this class is performed via the DnsMaster class, and is
// protected by the its lock.

#ifndef CHROME_BROWSER_NET_REFERRER_H_
#define CHROME_BROWSER_NET_REFERRER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

namespace chrome_browser_net {

//------------------------------------------------------------------------------
// For each hostname in a Referrer, we have a ReferrerValue.  It indicates
// exactly how much value (re: latency reduction) has resulted from having this
// entry.
class ReferrerValue {
 public:
  ReferrerValue() : birth_time_(base::Time::Now()) {}

  base::TimeDelta latency() const { return latency_; }
  base::Time birth_time() const { return birth_time_; }
  void AccrueValue(const base::TimeDelta& delta) { latency_ += delta; }
 private:
  base::TimeDelta latency_;  // Accumulated latency savings.
  const base::Time birth_time_;
};

//------------------------------------------------------------------------------
// A list of domain names to pre-resolve. The names are the keys to this map,
// and the values indicate the amount of benefit derived from having each name
// around.
typedef std::map<std::string, ReferrerValue> HostNameMap;

//------------------------------------------------------------------------------
// There is one Referrer instance for each hostname that has acted as an HTTP
// referer (note mispelling is intentional) for a hostname that was otherwise
// unexpectedly navgated towards ("unexpected" in the sense that the hostname
// was probably needad as a subresource of a page, and was not otherwise
// predictable until the content with the reference arrived).  Most typically,
// an outer page was a page fetched by the user, and this instance lists names
// in HostNameMap which are subresources and that were needed to complete the
// rendering of the outer page.
class Referrer : public HostNameMap {
 public:
  // Add the indicated host to the list of hosts that are resolved via DNS when
  // the user navigates to this referrer.  Note that if the list is long, an
  // entry may be discarded to make room for this insertion.
  void SuggestHost(const std::string& host);

  // Record additional usefulness of having this host name in the list.
  // Value is expressed as positive latency of amount delta.
  void AccrueValue(const base::TimeDelta& delta, const std::string host);

 private:
  // Helper function for pruning list.  Metric for usefulness is "large accrued
  // value," in the form of latency_ savings associated with a host name.  We
  // also give credit for a name being newly added, by scalling latency per
  // lifetime (time since birth).  For instance, when two names have accrued
  // the same latency_ savings, the older one is less valuable as it didn't
  // accrue savings as quickly.
  void DeleteLeastUseful();


  // We put these into a std::map<>, so we need copy constructors.
  // DISALLOW_COPY_AND_ASSIGN(Referrer);
  // TODO(jar): Consider optimization to use pointers to these instances, and
  // avoid deep copies during re-alloc of the containing map.
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_REFERRER_H_
