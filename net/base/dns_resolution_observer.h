// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file supports network stack independent notification of progress
// towards resolving a hostname.

// The observer class supports exactly one active (Add'ed) instance, and in
// typical usage, that observer will be Add'ed during process startup, and
// Remove'd during process termination.


#ifndef NET_BASE_DNS_RESOLUTION_OBSERVER_H_
#define NET_BASE_DNS_RESOLUTION_OBSERVER_H_

#include <string>

#include "net/base/host_resolver.h"

class GURL;

namespace net {

// TODO(eroman): Move this interface to HostResolver::Observer.
class DnsResolutionObserver {
 public:
  virtual ~DnsResolutionObserver() {}

  // For each OnStartResolution() notification, there should be a later
  // OnFinishResolutionWithStatus() indicating completion of the resolution
  // activity.
  // Related pairs of notification will arrive with matching id values.
  // A caller may use the id values to match up these asynchronous calls
  // from among a larger call stream.
  virtual void OnStartResolution(int id,
                                 const HostResolver::RequestInfo& info) = 0;
  virtual void OnFinishResolutionWithStatus(
      int id,
      bool was_resolved,
      const HostResolver::RequestInfo& info) = 0;
};

}  // namspace net

#endif  // NET_BASE_DNS_RESOLUTION_OBSERVER_H_
