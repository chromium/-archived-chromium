// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/string_util.h"
#include "base/path_service.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "net/proxy/proxy_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Javascript bindings for ProxyResolverV8, which returns mock values.
// Each time one of the bindings is called into, we push the input into a
// list, for later verification.
class MockJSBindings : public net::ProxyResolverV8::JSBindings {
 public:
  MockJSBindings() : my_ip_address_count(0) {}

  virtual void Alert(const std::string& message) {
    LOG(INFO) << "PAC-alert: " << message;  // Helpful when debugging.
    alerts.push_back(message);
  }

  virtual std::string MyIpAddress() {
    my_ip_address_count++;
    return my_ip_address_result;
  }

  virtual std::string DnsResolve(const std::string& host) {
    dns_resolves.push_back(host);
    return dns_resolve_result;
  }

  virtual void OnError(int line_number, const std::string& message) {
    // Helpful when debugging.
    LOG(INFO) << "PAC-error: [" << line_number << "] " << message;

    errors.push_back(message);
    errors_line_number.push_back(line_number);
  }

  // Mock values to return.
  std::string my_ip_address_result;
  std::string dns_resolve_result;

  // Inputs we got called with.
  std::vector<std::string> alerts;
  std::vector<std::string> errors;
  std::vector<int> errors_line_number;
  std::vector<std::string> dns_resolves;
  int my_ip_address_count;
};

// This is the same as ProxyResolverV8, but it uses mock bindings in place of
// the default bindings, and has a helper function to load PAC scripts from
// disk.
class ProxyResolverV8WithMockBindings : public net::ProxyResolverV8 {
 public:
  ProxyResolverV8WithMockBindings() : ProxyResolverV8(new MockJSBindings()) {}

  MockJSBindings* mock_js_bindings() const {
    return reinterpret_cast<MockJSBindings*>(js_bindings());
  }

  // Initialize with the PAC script data at |filename|.
  void SetPacScriptFromDisk(const char* filename) {
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
    SetPacScript(file_contents);
  }
};

// Doesn't really matter what these values are for many of the tests.
const GURL kQueryUrl("http://www.google.com");
const GURL kPacUrl;

}

TEST(ProxyResolverV8Test, Direct) {
  ProxyResolverV8WithMockBindings resolver;
  resolver.SetPacScriptFromDisk("direct.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  EXPECT_EQ(net::OK, result);
  EXPECT_TRUE(proxy_info.is_direct());

  EXPECT_EQ(0U, resolver.mock_js_bindings()->alerts.size());
  EXPECT_EQ(0U, resolver.mock_js_bindings()->errors.size());
}

TEST(ProxyResolverV8Test, ReturnEmptyString) {
  ProxyResolverV8WithMockBindings resolver;
  resolver.SetPacScriptFromDisk("return_empty_string.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  EXPECT_EQ(net::OK, result);
  EXPECT_TRUE(proxy_info.is_direct());

  EXPECT_EQ(0U, resolver.mock_js_bindings()->alerts.size());
  EXPECT_EQ(0U, resolver.mock_js_bindings()->errors.size());
}

TEST(ProxyResolverV8Test, Basic) {
  ProxyResolverV8WithMockBindings resolver;
  resolver.SetPacScriptFromDisk("passthrough.js");

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

    EXPECT_EQ(0U, resolver.mock_js_bindings()->alerts.size());
    EXPECT_EQ(0U, resolver.mock_js_bindings()->errors.size());
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
    ProxyResolverV8WithMockBindings resolver;
    resolver.SetPacScriptFromDisk(filenames[i]);

    net::ProxyInfo proxy_info;
    int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

    EXPECT_EQ(net::ERR_PAC_SCRIPT_FAILED, result);

    MockJSBindings* bindings = resolver.mock_js_bindings();
    EXPECT_EQ(0U, bindings->alerts.size());
    ASSERT_EQ(1U, bindings->errors.size());
    EXPECT_EQ("FindProxyForURL() did not return a string.",
              bindings->errors[0]);
    EXPECT_EQ(-1, bindings->errors_line_number[0]);
  }
}

// Try using a PAC script which defines no "FindProxyForURL" function.
TEST(ProxyResolverV8Test, NoEntryPoint) {
  ProxyResolverV8WithMockBindings resolver;
  resolver.SetPacScriptFromDisk("no_entrypoint.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  EXPECT_EQ(net::ERR_PAC_SCRIPT_FAILED, result);

  MockJSBindings* bindings = resolver.mock_js_bindings();
  EXPECT_EQ(0U, bindings->alerts.size());
  ASSERT_EQ(1U, bindings->errors.size());
  EXPECT_EQ("FindProxyForURL() is undefined.", bindings->errors[0]);
  EXPECT_EQ(-1, bindings->errors_line_number[0]);
}

// Try loading a malformed PAC script.
TEST(ProxyResolverV8Test, ParseError) {
  ProxyResolverV8WithMockBindings resolver;
  resolver.SetPacScriptFromDisk("missing_close_brace.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  EXPECT_EQ(net::ERR_PAC_SCRIPT_FAILED, result);

  MockJSBindings* bindings = resolver.mock_js_bindings();
  EXPECT_EQ(0U, bindings->alerts.size());

  // We get two errors -- one during compilation, and then later when
  // trying to run FindProxyForURL().
  ASSERT_EQ(2U, bindings->errors.size());

  EXPECT_EQ("Uncaught SyntaxError: Unexpected end of input",
            bindings->errors[0]);
  EXPECT_EQ(-1, bindings->errors_line_number[0]);

  EXPECT_EQ("FindProxyForURL() is undefined.", bindings->errors[1]);
  EXPECT_EQ(-1, bindings->errors_line_number[1]);
}

// Run a PAC script several times, which has side-effects.
TEST(ProxyResolverV8Test, SideEffects) {
  ProxyResolverV8WithMockBindings resolver;
  resolver.SetPacScriptFromDisk("side_effects.js");

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
  resolver.SetPacScriptFromDisk("side_effects.js");

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
  ProxyResolverV8WithMockBindings resolver;
  resolver.SetPacScriptFromDisk("unhandled_exception.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  EXPECT_EQ(net::ERR_PAC_SCRIPT_FAILED, result);

  MockJSBindings* bindings = resolver.mock_js_bindings();
  EXPECT_EQ(0U, bindings->alerts.size());
  ASSERT_EQ(1U, bindings->errors.size());
  EXPECT_EQ("Uncaught ReferenceError: undefined_variable is not defined",
            bindings->errors[0]);
  EXPECT_EQ(3, bindings->errors_line_number[0]);
}

// TODO(eroman): This test is disabed right now, since the parsing of
// host/port doesn't check for non-ascii characters.
TEST(ProxyResolverV8Test, DISABLED_ReturnUnicode) {
  ProxyResolverV8WithMockBindings resolver;
  resolver.SetPacScriptFromDisk("return_unicode.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  // The result from this resolve was unparseable, because it
  // wasn't ascii.
  EXPECT_EQ(net::ERR_PAC_SCRIPT_FAILED, result);
}

// Test the PAC library functions that we expose in the JS environmnet.
TEST(ProxyResolverV8Test, JavascriptLibrary) {
  ProxyResolverV8WithMockBindings resolver;
  resolver.SetPacScriptFromDisk("pac_library_unittest.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  // If the javascript side of this unit-test fails, it will throw a javascript
  // exception. Otherwise it will return "PROXY success:80".
  EXPECT_EQ(net::OK, result);
  EXPECT_EQ("success:80", proxy_info.proxy_server().ToURI());

  EXPECT_EQ(0U, resolver.mock_js_bindings()->alerts.size());
  EXPECT_EQ(0U, resolver.mock_js_bindings()->errors.size());
}

// Try resolving when SetPacScript() has not been called.
TEST(ProxyResolverV8Test, NoSetPacScript) {
  ProxyResolverV8WithMockBindings resolver;

  net::ProxyInfo proxy_info;

  // Resolve should fail, as we are not yet initialized with a script.
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);
  EXPECT_EQ(net::ERR_FAILED, result);

  // Initialize it.
  resolver.SetPacScriptFromDisk("direct.js");

  // Resolve should now succeed.
  result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);
  EXPECT_EQ(net::OK, result);

  // Clear it, by initializing with an empty string.
  resolver.SetPacScript(std::string());

  // Resolve should fail again now.
  result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);
  EXPECT_EQ(net::ERR_FAILED, result);

  // Load a good script once more.
  resolver.SetPacScriptFromDisk("direct.js");
  result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);
  EXPECT_EQ(net::OK, result);

  EXPECT_EQ(0U, resolver.mock_js_bindings()->alerts.size());
  EXPECT_EQ(0U, resolver.mock_js_bindings()->errors.size());
}

// Test marshalling/un-marshalling of values between C++/V8.
TEST(ProxyResolverV8Test, V8Bindings) {
  ProxyResolverV8WithMockBindings resolver;
  resolver.SetPacScriptFromDisk("bindings.js");

  net::ProxyInfo proxy_info;
  int result = resolver.GetProxyForURL(kQueryUrl, kPacUrl, &proxy_info);

  EXPECT_EQ(net::OK, result);
  EXPECT_TRUE(proxy_info.is_direct());

  MockJSBindings* bindings = resolver.mock_js_bindings();
  EXPECT_EQ(0U, resolver.mock_js_bindings()->errors.size());

  // Alert was called 5 times.
  ASSERT_EQ(5U, bindings->alerts.size());
  EXPECT_EQ("undefined", bindings->alerts[0]);
  EXPECT_EQ("null", bindings->alerts[1]);
  EXPECT_EQ("undefined", bindings->alerts[2]);
  EXPECT_EQ("[object Object]", bindings->alerts[3]);
  EXPECT_EQ("exception from calling toString()", bindings->alerts[4]);

  // DnsResolve was called 8 times.
  ASSERT_EQ(8U, bindings->dns_resolves.size());
  EXPECT_EQ("undefined", bindings->dns_resolves[0]);
  EXPECT_EQ("null", bindings->dns_resolves[1]);
  EXPECT_EQ("undefined", bindings->dns_resolves[2]);
  EXPECT_EQ("", bindings->dns_resolves[3]);
  EXPECT_EQ("[object Object]", bindings->dns_resolves[4]);
  EXPECT_EQ("function fn() {}", bindings->dns_resolves[5]);

  // TODO(eroman): This isn't quite right... should probably stringize
  // to something like "['3']".
  EXPECT_EQ("3", bindings->dns_resolves[6]);

  EXPECT_EQ("arg1", bindings->dns_resolves[7]);

  // MyIpAddress was called two times.
  EXPECT_EQ(2, bindings->my_ip_address_count);
}

TEST(ProxyResolverV8DefaultBindingsTest, DnsResolve) {
  // Get a hold of a DefaultJSBindings* (it is a hidden impl class).
  net::ProxyResolverV8 resolver;
  net::ProxyResolverV8::JSBindings* bindings = resolver.js_bindings();

  // Considered an error.
  EXPECT_EQ("", bindings->DnsResolve(""));

  const struct {
    const char* input;
    const char* expected;
  } tests[] = {
    {"www.google.com", "127.0.0.1"},
    {".", ""},
    {"foo@google.com", ""},
    {"@google.com", ""},
    {"foo:pass@google.com", ""},
    {"%", ""},
    {"www.google.com:80", ""},
    {"www.google.com:", ""},
    {"www.google.com.", ""},
    {"#", ""},
    {"127.0.0.1", ""},
    {"this has spaces", ""},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string actual = bindings->DnsResolve(tests[i].input);

    // ########################################################################
    // TODO(eroman)
    // ########################################################################
    // THIS TEST IS CURRENTLY FLAWED.
    //
    // Since we are running in unit-test mode, the HostResolve is using a
    // mock HostMapper, which will always return 127.0.0.1, without going
    // through the real codepaths.
    //
    // It is important that these tests be run with the real thing, since we
    // need to verify that HostResolver doesn't blow up when you send it
    // weird inputs. This is necessary since the data reach it is UNSANITIZED.
    // It comes directly from the PAC javascript.
    //
    // For now we just check that it maps to 127.0.0.1.
    std::string expected = tests[i].expected;
    if (expected == "")
      expected = "127.0.0.1";
    EXPECT_EQ(expected, actual);
  }
}

TEST(ProxyResolverV8DefaultBindingsTest, MyIpAddress) {
  // Get a hold of a DefaultJSBindings* (it is a hidden impl class).
  net::ProxyResolverV8 resolver;
  net::ProxyResolverV8::JSBindings* bindings = resolver.js_bindings();

  // Our ip address is always going to be 127.0.0.1, since we are using a
  // mock host mapper when running in unit-test mode.
  std::string my_ip_address = bindings->MyIpAddress();

  EXPECT_EQ("127.0.0.1", my_ip_address);
}
