// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_SIMPLE_RESOURCE_LOADER_BRIDGE_H__
#define WEBKIT_TOOLS_TEST_SHELL_SIMPLE_RESOURCE_LOADER_BRIDGE_H__

#include <string>

class GURL;
class URLRequestContext;

class SimpleResourceLoaderBridge {
 public:
  // Call this function to initialize the simple resource loader bridge.  If
  // the given context is null, then a default TestShellRequestContext will be
  // instantiated.  Otherwise, a reference is taken to the given request
  // context, which will be released when Shutdown is called.  The caller
  // should not hold another reference to the request context!  It is safe to
  // call this function multiple times.
  //
  // NOTE: If this function is not called, then a default request context will
  // be initialized lazily.
  //
  static void Init(URLRequestContext* context);

  // Call this function to shutdown the simple resource loader bridge.
  static void Shutdown();

  // May only be called after Init.
  static void SetCookie(const GURL& url,
                        const GURL& first_party_for_cookies,
                        const std::string& cookie);
  static std::string GetCookies(const GURL& url,
                                const GURL& first_party_for_cookies);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_SIMPLE_RESOURCE_LOADER_BRIDGE_H__
