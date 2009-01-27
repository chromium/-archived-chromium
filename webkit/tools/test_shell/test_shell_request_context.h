// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_REQUEST_CONTEXT_H__
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_REQUEST_CONTEXT_H__

#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"

// A basic URLRequestContext that only provides an in-memory cookie store.
class TestShellRequestContext : public URLRequestContext {
 public:
  // Use an in-memory cache
  TestShellRequestContext();

  // Use an on-disk cache at the specified location.  Optionally, use the cache
  // in playback or record mode.
  TestShellRequestContext(const std::wstring& cache_path,
                          net::HttpCache::Mode cache_mode,
                          bool no_proxy);

  ~TestShellRequestContext();

  virtual const std::string& GetUserAgent(const GURL& url) const;

 private:
  void Init(const std::wstring& cache_path, net::HttpCache::Mode cache_mode,
            bool no_proxy);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_REQUEST_CONTEXT_H__

