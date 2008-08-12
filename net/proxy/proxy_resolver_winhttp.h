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

#ifndef NET_PROXY_PROXY_RESOLVER_WINHTTP_H_
#define NET_PROXY_PROXY_RESOLVER_WINHTTP_H_

#include "net/proxy/proxy_service.h"

typedef LPVOID HINTERNET;  // From winhttp.h

namespace net {

// An implementation of ProxyResolver that uses WinHTTP and the system
// proxy settings.
class ProxyResolverWinHttp : public ProxyResolver {
 public:
  ProxyResolverWinHttp();
  ~ProxyResolverWinHttp();

  // ProxyResolver implementation:
  virtual int GetProxyConfig(ProxyConfig* config);
  virtual int GetProxyForURL(const std::string& query_url,
                             const std::string& pac_url,
                             ProxyInfo* results);

 private:
   bool OpenWinHttpSession();
   void CloseWinHttpSession();

  // Proxy configuration is cached on the session handle.
  HINTERNET session_handle_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverWinHttp);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_WINHTTP_H_
