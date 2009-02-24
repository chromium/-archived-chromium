// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Initialize |resolver| with the PAC script data at |filename|.
void InitWithScriptFromDisk(net::ProxyResolver* resolver,
                            const char* filename) {
  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("net");
  path = path.AppendASCII("data");
  path = path.AppendASCII("proxy_resolver_v8_unittest");
  path = path.AppendASCII(filename);

  // Try to read the file from disk.
  std::string file_contents;
  bool ok = file_util::ReadFileToString(path, &file_contents);

  // If we can't load the file from disk, something is misconfigured.
  LOG_IF(ERROR, !ok) << "Failed to read file: " << path.value();
  ASSERT_TRUE(ok);

  // Load the PAC script into the ProxyResolver.
  resolver->SetPacScript(file_contents);
}

// Doesn't really matter what these values are for many of the tests.
const GURL kQueryUrl("http://www.google.com");
const GURL kPacUrl;

}

TEST(ProxyResolverV8Test, Direct) {
  net::ProxyResolverV8 resolver;
  InitWithScriptFromDisk(&resolver, "direct.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  EXPECT_EQ(net::OK, result);
  EXPECT_TRUE(proxy_info.is_direct());
}

TEST(ProxyResolverV8Test, ReturnEmptyString) {
  net::ProxyResolverV8 resolver;
  InitWithScriptFromDisk(&resolver, "return_empty_string.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  EXPECT_EQ(net::OK, result);
  EXPECT_TRUE(proxy_info.is_direct());
}

TEST(ProxyResolverV8Test, Basic) {
  net::ProxyResolverV8 resolver;
  InitWithScriptFromDisk(&resolver, "passthrough.js");

  // The "FindProxyForURL" of this PAC script simply concatenates all of the
  // arguments into a pseudo-host. The purpose of this test is to verify that
  // the correct arguments are being passed to FindProxyForURL().
  {
    net::ProxyInfo proxy_info;
    int result = resolver.GetProxyForURL(GURL("http://query.com/path"),
                                         kPacUrl, &proxy_info);
    EXPECT_EQ(net::OK, result);
    EXPECT_EQ("http.query.com.path.query.com:80",
              proxy_info.proxy_server().ToURI());
  }
  {
    net::ProxyInfo proxy_info;
    int result = resolver.GetProxyForURL(GURL("ftp://query.com:90/path"),
                                         kPacUrl, &proxy_info);
    EXPECT_EQ(net::OK, result);
    // Note that FindProxyForURL(url, host) does not expect |host| to contain
    // the port number.
    EXPECT_EQ("ftp.query.com.90.path.query.com:80",
              proxy_info.proxy_server().ToURI());
  }
}

TEST(ProxyResolverV8Test, BadReturnType) {
  // These are the filenames of PAC scripts which each return a non-string
  // types for FindProxyForURL(). They should all fail with
  // ERR_PAC_SCRIPT_FAILED.
  static const char* const filenames[] = {
      "return_undefined.js",
      "return_integer.js",
      "return_function.js",
      "return_object.js",
      // TODO(eroman): Should 'null' be considered equivalent to "DIRECT" ?
      "return_null.js"
  };

  for (size_t i = 0; i < arraysize(filenames); ++i) {
    net::ProxyResolverV8 resolver;
    InitWithScriptFromDisk(&resolver, filenames[i]);

    net::ProxyInfo proxy_info;
    int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

    EXPECT_EQ(net::ERR_PAC_SCRIPT_FAILED, result);
  }
}

// Try using a PAC script which defines no "FindProxyForURL" function.
TEST(ProxyResolverV8Test, NoEntryPoint) {
  net::ProxyResolverV8 resolver;
  InitWithScriptFromDisk(&resolver, "no_entrypoint.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  EXPECT_EQ(net::ERR_PAC_SCRIPT_FAILED, result);
}

// Try loading a malformed PAC script.
TEST(ProxyResolverV8Test, ParseError) {
  net::ProxyResolverV8 resolver;
  InitWithScriptFromDisk(&resolver, "missing_close_brace.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  EXPECT_EQ(net::ERR_PAC_SCRIPT_FAILED, result);
}

// Run a PAC script several times, which has side-effects.
TEST(ProxyResolverV8Test, SideEffects) {
  net::ProxyResolverV8 resolver;
  InitWithScriptFromDisk(&resolver, "side_effects.js");

  // The PAC script increments a counter each time we invoke it.
  for (int i = 0; i < 3; ++i) {
    net::ProxyInfo proxy_info;
    int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);
    EXPECT_EQ(net::OK, result);
    EXPECT_EQ(StringPrintf("sideffect_%d:80", i),
              proxy_info.proxy_server().ToURI());
  }

  // Reload the script -- the javascript environment should be reset, hence
  // the counter starts over.
  InitWithScriptFromDisk(&resolver, "side_effects.js");

  for (int i = 0; i < 3; ++i) {
    net::ProxyInfo proxy_info;
    int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);
    EXPECT_EQ(net::OK, result);
    EXPECT_EQ(StringPrintf("sideffect_%d:80", i),
              proxy_info.proxy_server().ToURI());
  }
}

// Execute a PAC script which throws an exception in FindProxyForURL.
TEST(ProxyResolverV8Test, UnhandledException) {
  net::ProxyResolverV8 resolver;
  InitWithScriptFromDisk(&resolver, "unhandled_exception.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  EXPECT_EQ(net::ERR_PAC_SCRIPT_FAILED, result);
}

// TODO(eroman): This test is disabed right now, since the parsing of
// host/port doesn't check for non-ascii characters.
TEST(ProxyResolverV8Test, DISABLED_ReturnUnicode) {
  net::ProxyResolverV8 resolver;
  InitWithScriptFromDisk(&resolver, "return_unicode.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  // The result from this resolve was unparseable, because it
  // wasn't ascii.
  EXPECT_EQ(net::ERR_PAC_SCRIPT_FAILED, result);
}

// Test the PAC library functions that we expose in the JS environmnet.
TEST(ProxyResolverV8Test, JavascriptLibrary) {
  net::ProxyResolverV8 resolver;
  InitWithScriptFromDisk(&resolver, "pac_library_unittest.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  // If the javascript side of this unit-test fails, it will throw a javascript
  // exception. Otherwise it will return "PROXY success:80".
  EXPECT_EQ(net::OK, result);
  EXPECT_EQ("success:80", proxy_info.proxy_server().ToURI());
}

// Try resolving when SetPacScript() has not been called.
TEST(ProxyResolverV8Test, NoSetPacScript) {
  net::ProxyResolverV8 resolver;

  net::ProxyInfo proxy_info;

  // Resolve should fail, as we are not yet initialized with a script.
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);
  EXPECT_EQ(net::ERR_FAILED, result);

  // Initialize it.
  InitWithScriptFromDisk(&resolver, "direct.js");

  // Resolve should now succeed.
  result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);
  EXPECT_EQ(net::OK, result);

  // Clear it, by initializing with an empty string.
  resolver.SetPacScript(std::string());

  // Resolve should fail again now.
  result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);
  EXPECT_EQ(net::ERR_FAILED, result);

  // Load a good script once more.
  InitWithScriptFromDisk(&resolver, "direct.js");
  result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);
  EXPECT_EQ(net::OK, result);
}