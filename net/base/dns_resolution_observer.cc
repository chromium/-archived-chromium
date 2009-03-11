// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file supports network stack independent notification of progress
// towards resolving a hostname.

#include "net/base/dns_resolution_observer.h"

#include <string>

#include "base/logging.h"

namespace net {

static DnsResolutionObserver* dns_resolution_observer;

void AddDnsResolutionObserver(DnsResolutionObserver* new_observer) {
  if (new_observer == dns_resolution_observer)
    return;  // Facilitate unit tests that init/teardown repeatedly.
  DCHECK(!dns_resolution_observer);
  dns_resolution_observer = new_observer;
}

DnsResolutionObserver* RemoveDnsResolutionObserver() {
  // We really need to check that the entire network subsystem is shutting down,
  // and hence no additional calls can even *possibly* still be lingering in the
  // notification path that includes our observer.  Until we have a way to
  // really assert that fact, we will outlaw the calling of this function.
  // Darin suggested that the caller use a static initializer for the observer,
  // so that it can safely be destroyed after process termination, and without
  // inducing a memory leak.
  // Bottom line: Don't call this function!  You will crash for now.
  CHECK(0);
  DnsResolutionObserver* old_observer = dns_resolution_observer;
  dns_resolution_observer = NULL;
  return old_observer;
}

// Locking access to dns_resolution_observer is not really critical... but we
// should test the value of dns_resolution_observer that we use.
// Worst case, we'll get an "out of date" value... which is no big deal for the
// DNS prefetching system (the most common (only?) observer).
void DidStartDnsResolution(const std::string& name, void* context) {
  DnsResolutionObserver* current_observer = dns_resolution_observer;
  if (current_observer)
    current_observer->OnStartResolution(name, context);
}

void DidFinishDnsResolutionWithStatus(bool was_resolved,
                                      const GURL& referrer,
                                      void* context) {
  DnsResolutionObserver* current_observer = dns_resolution_observer;
  if (current_observer) {
    current_observer->OnFinishResolutionWithStatus(was_resolved, referrer,
                                                   context);
  }
}

}  // namspace net
