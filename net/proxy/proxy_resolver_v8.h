// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_V8_H_
#define NET_PROXY_PROXY_RESOLVER_V8_H_

#include <string>

#include "base/scoped_ptr.h"
#include "net/proxy/proxy_service.h"

namespace net {

// Implementation of ProxyResolver that uses V8 to evaluate PAC scripts.
//
// ----------------------------------------------------------------------------
// !!! Important note on threading model:
// ----------------------------------------------------------------------------
// There can be only one instance of V8 running at a time. To enforce this
// constraint, ProxyResolverV8 holds a v8::Locker during execution. Therefore
// it is OK to run multiple instances of ProxyResolverV8 on different threads,
// since only one will be running inside V8 at a time.
//
// It is important that *ALL* instances of V8 in the process be using
// v8::Locker. If not there can be race conditions beween the non-locked V8
// instances and the locked V8 instances used by ProxyResolverV8 (assuming they
// run on different threads).
//
// This is the case with the V8 instance used by chromium's renderer -- it runs
// on a different thread from ProxyResolver (renderer thread vs PAC thread),
// and does not use locking since it expects to be alone.
class ProxyResolverV8 : public ProxyResolver {
 public:
  ProxyResolverV8();
  ~ProxyResolverV8();

  // ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& query_url,
                             const GURL& /*pac_url*/,
                             ProxyInfo* results);
  virtual void SetPacScript(const std::string& bytes);

 private:
  // Context holds the Javascript state for the most recently loaded PAC
  // script. It corresponds with the data from the last call to
  // SetPacScript().
  class Context;
  scoped_ptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverV8);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_V8_H_
