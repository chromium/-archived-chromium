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

#include "googleurl/src/gurl.h"

namespace net {

class DnsResolutionObserver {
 public:
  virtual ~DnsResolutionObserver() {}

  // For each OnStartResolution() notification, there should be a later
  // OnFinishResolutionWithStatus() indicating completion of the resolution
  // activity.
  // Related pairs of notification will arrive with matching context values.
  // A caller may use the context values to match up these asynchronous calls
  // from among a larger call stream.
  // Once a matching pair of notifications has been provided (i.e., a pair with
  // identical context values), and the notification methods (below) have
  // returned, the context values *might* be reused.
  virtual void OnStartResolution(const std::string& host_name,
                                 void* context) = 0;
  virtual void OnFinishResolutionWithStatus(bool was_resolved,
                                            const GURL& referrer,
                                            void* context) = 0;
};


// Note that *exactly* one observer is currently supported, and any attempt to
// add a second observer via AddDnsResolutionObserver() before removing the
// first DnsResolutionObserver will induce a DCHECK() assertion.
void AddDnsResolutionObserver(DnsResolutionObserver* new_observer);

// Note that the RemoveDnsResolutionObserver() will NOT perform any delete
// operations, and it is the responsibility of the code that called
// AddDnsResolutionObserver() to make a corresponding call to
// RemoveDnsResolutionObserver() and then delete the returned
// DnsResolutionObserver instance.
DnsResolutionObserver* RemoveDnsResolutionObserver();

// The following functions are expected to be called only by network stack
// implementations.  This above observer class will relay the notifications
// to any registered observer.
void DidStartDnsResolution(const std::string& name,
                           void* context);
void DidFinishDnsResolutionWithStatus(bool was_resolved,
                                      const GURL& url,
                                      void* context);
}  // namspace net

#endif  // NET_BASE_DNS_RESOLUTION_OBSERVER_H_
