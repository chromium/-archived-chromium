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

#include "webkit/tools/test_shell/test_shell_request_context.h"

#include "net/base/cookie_monster.h"
#include "webkit/glue/webkit_glue.h"

TestShellRequestContext::TestShellRequestContext() {
  Init(std::wstring(), net::HttpCache::NORMAL);
}

TestShellRequestContext::TestShellRequestContext(
    const std::wstring& cache_path,
    net::HttpCache::Mode cache_mode) {
  Init(cache_path, cache_mode);
}

void TestShellRequestContext::Init(
    const std::wstring& cache_path,
    net::HttpCache::Mode cache_mode) {
  cookie_store_ = new CookieMonster();

  user_agent_ = webkit_glue::GetDefaultUserAgent();

  // hard-code A-L and A-C for test shells
  accept_language_ = "en-us,en";
  accept_charset_ = "iso-8859-1,*,utf-8";

  net::HttpCache *cache;
  if (cache_path.empty()) {
    cache = new net::HttpCache(NULL, 0);
  } else {
    cache = new net::HttpCache(NULL, cache_path, 0);
  }
  cache->set_mode(cache_mode);
  http_transaction_factory_ = cache;
}

TestShellRequestContext::~TestShellRequestContext() {
  delete cookie_store_;
  delete http_transaction_factory_;
}
