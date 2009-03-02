// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_V8_H_
#define NET_PROXY_PROXY_RESOLVER_V8_H_

#include <string>

#include "base/scoped_ptr.h"
#include "net/proxy/proxy_resolver.h"

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
  // Constructs a ProxyResolverV8 with default javascript bindings.
  //
  // The default javascript bindings will:
  //   - Send script error messages to LOG(INFO)
  //   - Send script alert()s to LOG(INFO)
  //   - Use the default host mapper to service dnsResolve(), synchronously
  //     on the V8 thread.
  //
  // For clients that need more control (for example, sending the script output
  // to a UI widget), use the ProxyResolverV8(JSBindings*) and specify your
  // own bindings.
  ProxyResolverV8();

  class JSBindings;

  // Constructs a ProxyResolverV8 with custom bindings. ProxyResolverV8 takes
  // ownership of |custom_js_bindings| and deletes it when ProxyResolverV8
  // is destroyed.
  explicit ProxyResolverV8(JSBindings* custom_js_bindings);

  ~ProxyResolverV8();

  // ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& query_url,
                             const GURL& /*pac_url*/,
                             ProxyInfo* results);
  virtual void SetPacScript(const std::string& bytes);

  JSBindings* js_bindings() const { return js_bindings_.get(); }

 private:
  // Context holds the Javascript state for the most recently loaded PAC
  // script. It corresponds with the data from the last call to
  // SetPacScript().
  class Context;
  scoped_ptr<Context> context_;

  scoped_ptr<JSBindings> js_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverV8);
};

// Interface for the javascript bindings.
class ProxyResolverV8::JSBindings {
 public:
  virtual ~JSBindings() {}

  // Handler for "alert(message)"
  virtual void Alert(const std::string& message) = 0;

  // Handler for "myIpAddress()". Returns empty string on failure. 
  virtual std::string MyIpAddress() = 0;      

  // Handler for "dnsResolve(host)". Returns empty string on failure.
  virtual std::string DnsResolve(const std::string& host) = 0;

  // Handler for when an error is encountered. |line_number| may be -1
  // if a line number is not applicable to this error.
  virtual void OnError(int line_number, const std::string& error) = 0;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_V8_H_
