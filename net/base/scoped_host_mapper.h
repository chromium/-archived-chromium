// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines ScopedHostMapper, which is a helper class for writing
// tests that use HostResolver either directly or indirectly.
//
// In most cases, it is important that unit tests avoid making actual DNS
// queries since the resulting tests can be flaky, especially if the network is
// unreliable for some reason.  To simplify writing tests that avoid making
// actual DNS queries, the following helper class may be used:
//
//   ScopedHostMapper scoped_host_mapper;
//   scoped_host_mapper.AddRule("foo.com", "1.2.3.4");
//   scoped_host_mapper.AddRule("bar.com", "2.3.4.5");
//   ...
//
// The above rules define a static mapping from hostnames to IP address
// literals.  The first parameter to AddRule specifies a host pattern to match
// against, and the second parameter indicates what value should be used to
// replace the given hostname.  So, the following is also supported:
//
//   scoped_host_mapper.AddRule("*.com", "127.0.0.1");
//
// If there are multiple ScopedHostMappers in existence, then the last one
// allocated will be used.  However, if it does not provide a matching rule,
// then it will delegate to the previously set HostMapper (see SetHostMapper).
// Finally, if no HostMapper matches a given hostname, then the hostname will
// be unmodified.
//
// IMPORTANT: ScopedHostMapper is only designed to be used on a single thread,
// and it is a requirement that the lifetimes of multiple instances be nested.

#ifndef NET_BASE_SCOPED_HOST_MAPPER_H_
#define NET_BASE_SCOPED_HOST_MAPPER_H_

#ifdef UNIT_TEST

#include <list>

#include "base/string_util.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"

namespace net {

// This class sets the HostMapper for a particular scope.
class ScopedHostMapper : public HostMapper {
 public:
  ScopedHostMapper() {
    previous_host_mapper_ = SetHostMapper(this);
  }

  ~ScopedHostMapper() {
    HostMapper* old_mapper = SetHostMapper(previous_host_mapper_);
    // The lifetimes of multiple instances must be nested.
    CHECK(old_mapper == this);
  }

  // Any hostname matching the given pattern will be replaced with the given
  // replacement value.  Usually, replacement should be an IP address literal.
  void AddRule(const char* host_pattern, const char* replacement) {
    rules_.push_back(Rule(host_pattern, replacement));
  }

 private:
  std::string Map(const std::string& host) {
    RuleList::const_iterator r;
    for (r = rules_.begin(); r != rules_.end(); ++r) {
      if (MatchPattern(host, r->host_pattern))
        return r->replacement;
    }
    return previous_host_mapper_ ? previous_host_mapper_->Map(host) : host;
  }

  struct Rule {
    std::string host_pattern;
    std::string replacement;
    Rule(const char* h, const char* r) : host_pattern(h), replacement(r) {}
  };
  typedef std::list<Rule> RuleList;

  HostMapper* previous_host_mapper_;
  RuleList rules_;
};

}  // namespace net

#endif  // UNIT_TEST

#endif  // NET_BASE_SCOPED_HOST_MAPPER_H_
