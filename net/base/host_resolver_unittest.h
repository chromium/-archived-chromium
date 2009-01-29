// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HOST_RESOLVER_UNITTEST_H_
#define NET_BASE_HOST_RESOLVER_UNITTEST_H_

#ifdef UNIT_TEST

#include <list>

#include "base/string_util.h"
#include "base/platform_thread.h"
#include "base/ref_counted.h"
#include "base/waitable_event.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"

namespace net {

// In most cases, it is important that unit tests avoid making actual DNS
// queries since the resulting tests can be flaky, especially if the network is
// unreliable for some reason.  To simplify writing tests that avoid making
// actual DNS queries, the following helper class may be used:
//
//   scoped_refptr<RuleBasedHostMapper> host_mapper = new RuleBasedHostMapper();
//   host_mapper->AddRule("foo.com", "1.2.3.4");
//   host_mapper->AddRule("bar.com", "2.3.4.5");
//
// Don't forget to actually set your mapper, probably with ScopedHostMapper!
//
// The above rules define a static mapping from hostnames to IP address
// literals.  The first parameter to AddRule specifies a host pattern to match
// against, and the second parameter indicates what value should be used to
// replace the given hostname.  So, the following is also supported:
//
//   host_mapper->AddRule("*.com", "127.0.0.1");
//
// Replacement doesn't have to be string representing an IP address. It can
// re-map one hostname to another as well.
class RuleBasedHostMapper : public HostMapper {
 public:
  // Any hostname matching the given pattern will be replaced with the given
  // replacement value.  Usually, replacement should be an IP address literal.
  void AddRule(const char* host_pattern, const char* replacement) {
    rules_.push_back(Rule(host_pattern, replacement));
  }

  void AddRuleWithLatency(const char* host_pattern, const char* replacement,
                          int latency) {
    rules_.push_back(Rule(host_pattern, replacement, latency));
  }

 private:
  std::string Map(const std::string& host) {
    RuleList::iterator r;
    for (r = rules_.begin(); r != rules_.end(); ++r) {
      if (MatchPattern(host, r->host_pattern)) {
        if (r->latency != 0) {
          PlatformThread::Sleep(r->latency);
          r->latency = 1;  // Simulate cache warmup.
        }
        return r->replacement;
      }
    }
    return MapUsingPrevious(host);
  }

  struct Rule {
    std::string host_pattern;
    std::string replacement;
    int latency;  // in milliseconds
    Rule(const char* h, const char* r)
        : host_pattern(h),
          replacement(r),
          latency(0) {}
    Rule(const char* h, const char* r, const int l)
        : host_pattern(h),
          replacement(r),
          latency(l) {}
  };
  typedef std::list<Rule> RuleList;

  RuleList rules_;
};

// Using WaitingHostMapper you can simulate very long lookups, for example
// to test code which cancels a request. Example usage:
//
//   scoped_refptr<WaitingHostMapper> mapper = new WaitingHostMapper();
//   ScopedHostMapper scoped_mapper(mapper.get());
//
//   (start the lookup asynchronously)
//   (cancel the lookup)
//
//   mapper->Signal();
class WaitingHostMapper : public HostMapper {
 public:
  WaitingHostMapper() : event_(false, false) {
  }

  void Signal() {
    event_.Signal();
  }

 private:
  std::string Map(const std::string& host) {
    event_.Wait();
    return MapUsingPrevious(host);
  }

  base::WaitableEvent event_;
};

// This class sets the HostMapper for a particular scope. If there are multiple
// ScopedHostMappers in existence, then the last one allocated will be used.
// However, if it does not provide a matching rule, then it should delegate
// to the previously set HostMapper (see SetHostMapper). This is true for all
// mappers defined in this file. If no HostMapper matches a given hostname, then
// the hostname will be unmodified.
class ScopedHostMapper {
 public:
  ScopedHostMapper(HostMapper* mapper) : current_host_mapper_(mapper) {
    previous_host_mapper_ = SetHostMapper(current_host_mapper_.get());
    current_host_mapper_->set_previous_mapper(previous_host_mapper_.get());
  }

  ~ScopedHostMapper() {
    HostMapper* old_mapper = SetHostMapper(previous_host_mapper_.get());
    // The lifetimes of multiple instances must be nested.
    CHECK(old_mapper == current_host_mapper_.get());
  }

 private:
  scoped_refptr<HostMapper> current_host_mapper_;
  scoped_refptr<HostMapper> previous_host_mapper_;
};

}  // namespace net

#endif  // UNIT_TEST

#endif  // NET_BASE_HOST_RESOLVER_UNITTEST_H_
