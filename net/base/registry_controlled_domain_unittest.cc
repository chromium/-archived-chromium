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

#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestRegistryControlledDomainService;
static TestRegistryControlledDomainService* test_instance_;

class TestRegistryControlledDomainService :
    public net::RegistryControlledDomainService {
 public:

  // Deletes the instance so a new one will be created.
  static void ResetInstance() {
    net::RegistryControlledDomainService::ResetInstance();
  }

  // Sets and parses the given data.
  static void UseDomainData(const std::string& data) {
    net::RegistryControlledDomainService::UseDomainData(data);
  }

 private:
  TestRegistryControlledDomainService::TestRegistryControlledDomainService() { }
  TestRegistryControlledDomainService::~TestRegistryControlledDomainService() {
  }
};

class RegistryControlledDomainTest : public testing::Test {
 protected:
  virtual void SetUp() {
    TestRegistryControlledDomainService::ResetInstance();
  }
};

// Convenience functions to shorten the names for repeated use below.
void SetTestData(const std::string& data) {
  TestRegistryControlledDomainService::UseDomainData(data);
}

std::string GetDomainFromURL(const std::string& url) {
  return TestRegistryControlledDomainService::GetDomainAndRegistry(GURL(url));
}

std::string GetDomainFromHost(const std::wstring& host) {
  return TestRegistryControlledDomainService::GetDomainAndRegistry(host);
}

size_t GetRegistryLengthFromURL(const std::string& url,
                                bool allow_unknown_registries) {
  return TestRegistryControlledDomainService::GetRegistryLength(GURL(url),
      allow_unknown_registries);
}

size_t GetRegistryLengthFromHost(const std::wstring& host,
                                 bool allow_unknown_registries) {
  return TestRegistryControlledDomainService::GetRegistryLength(host,
      allow_unknown_registries);
}

bool CompareDomains(const std::string& url1, const std::string& url2) {
  GURL g1 = GURL(url1);
  GURL g2 = GURL(url2);
  return TestRegistryControlledDomainService::SameDomainOrHost(g1, g2);
}

} // namespace

TEST_F(RegistryControlledDomainTest, TestParsing) {
  // Ensure that various simple and pathological cases parse without hanging or
  // crashing.  Testing the correctness of the parsing directly would require
  // opening the singleton class up more.
  SetTestData("com");
  SetTestData("abc.com\n");
  SetTestData("abc.com\ndef.com\n*.abc.com\n!foo.abc.com");
  SetTestData("abc.com.\n");
  SetTestData("");
  SetTestData("*.");
  SetTestData("!");
  SetTestData(".");
}

static const char kTestData[] = "jp\n"            // 1
                                "ac.jp\n"         // 2
                                "*.bar.jp\n"      // 3
                                "*.baz.bar.jp\n"  // 4
                                "*.foo.bar.jp\n"  // 5
                                "!foo.bar.jp\n"   // 6
                                "!pref.bar.jp\n"  // 7
                                "bar.baz.com\n"   // 8
                                "*.c\n"           // 9
                                "!b.c";           // 10

TEST_F(RegistryControlledDomainTest, TestGetDomainAndRegistry) {
  SetTestData(kTestData);

  // Test GURL version of GetDomainAndRegistry().
  EXPECT_EQ("baz.jp", GetDomainFromURL("http://a.baz.jp/file.html"));   // 1
  EXPECT_EQ("baz.jp.", GetDomainFromURL("http://a.baz.jp./file.html")); // 1
  EXPECT_EQ("", GetDomainFromURL("http://ac.jp"));                      // 2
  EXPECT_EQ("", GetDomainFromURL("http://a.bar.jp"));                   // 3
  EXPECT_EQ("", GetDomainFromURL("http://bar.jp"));                     // 3
  EXPECT_EQ("", GetDomainFromURL("http://baz.bar.jp"));                 // 3 4
  EXPECT_EQ("a.b.baz.bar.jp", GetDomainFromURL("http://a.b.baz.bar.jp"));
                                                                        // 4
  EXPECT_EQ("foo.bar.jp", GetDomainFromURL("http://foo.bar.jp"));       // 3 5 6
  EXPECT_EQ("pref.bar.jp", GetDomainFromURL("http://baz.pref.bar.jp")); // 7
  EXPECT_EQ("b.bar.baz.com.", GetDomainFromURL("http://a.b.bar.baz.com."));
                                                                        // 8
  EXPECT_EQ("a.d.c", GetDomainFromURL("http://a.d.c"));                 // 9
  EXPECT_EQ("a.d.c", GetDomainFromURL("http://.a.d.c"));                // 9
  EXPECT_EQ("a.d.c", GetDomainFromURL("http://..a.d.c"));               // 9
  EXPECT_EQ("b.c", GetDomainFromURL("http://a.b.c"));                   // 9 10
  EXPECT_EQ("baz.com", GetDomainFromURL("http://baz.com"));             // none
  EXPECT_EQ("baz.com.", GetDomainFromURL("http://baz.com."));           // none

  EXPECT_EQ("", GetDomainFromURL(""));
  EXPECT_EQ("", GetDomainFromURL("http://"));
  EXPECT_EQ("", GetDomainFromURL("file:///C:/file.html"));
  EXPECT_EQ("", GetDomainFromURL("http://foo.com.."));
  EXPECT_EQ("", GetDomainFromURL("http://..."));
  EXPECT_EQ("", GetDomainFromURL("http://192.168.0.1"));
  EXPECT_EQ("", GetDomainFromURL("http://localhost"));
  EXPECT_EQ("", GetDomainFromURL("http://localhost."));
  EXPECT_EQ("", GetDomainFromURL("http:////Comment"));

  // Test std::wstring version of GetDomainAndRegistry().  Uses the same
  // underpinnings as the GURL version, so this is really more of a check of
  // CanonicalizeHost().
  EXPECT_EQ("baz.jp", GetDomainFromHost(L"a.baz.jp"));                 // 1
  EXPECT_EQ("baz.jp.", GetDomainFromHost(L"a.baz.jp."));               // 1
  EXPECT_EQ("", GetDomainFromHost(L"ac.jp"));                          // 2
  EXPECT_EQ("", GetDomainFromHost(L"a.bar.jp"));                       // 3
  EXPECT_EQ("", GetDomainFromHost(L"bar.jp"));                         // 3
  EXPECT_EQ("", GetDomainFromHost(L"baz.bar.jp"));                     // 3 4
  EXPECT_EQ("a.b.baz.bar.jp", GetDomainFromHost(L"a.b.baz.bar.jp"));   // 3 4
  EXPECT_EQ("foo.bar.jp", GetDomainFromHost(L"foo.bar.jp"));           // 3 5 6
  EXPECT_EQ("pref.bar.jp", GetDomainFromHost(L"baz.pref.bar.jp"));     // 7
  EXPECT_EQ("b.bar.baz.com.", GetDomainFromHost(L"a.b.bar.baz.com.")); // 8
  EXPECT_EQ("a.d.c", GetDomainFromHost(L"a.d.c"));                     // 9
  EXPECT_EQ("a.d.c", GetDomainFromHost(L".a.d.c"));                    // 9
  EXPECT_EQ("a.d.c", GetDomainFromHost(L"..a.d.c"));                   // 9
  EXPECT_EQ("b.c", GetDomainFromHost(L"a.b.c"));                       // 9 10
  EXPECT_EQ("baz.com", GetDomainFromHost(L"baz.com"));                 // none
  EXPECT_EQ("baz.com.", GetDomainFromHost(L"baz.com."));               // none

  EXPECT_EQ("", GetDomainFromHost(L""));
  EXPECT_EQ("", GetDomainFromHost(L"foo.com.."));
  EXPECT_EQ("", GetDomainFromHost(L"..."));
  EXPECT_EQ("", GetDomainFromHost(L"192.168.0.1"));
  EXPECT_EQ("", GetDomainFromHost(L"localhost."));
  EXPECT_EQ("", GetDomainFromHost(L".localhost."));
}

TEST_F(RegistryControlledDomainTest, TestGetRegistryLength) {
  SetTestData(kTestData);

  // Test GURL version of GetRegistryLength().
  EXPECT_EQ(2, GetRegistryLengthFromURL("http://a.baz.jp/file.html", false));
                                                                        // 1
  EXPECT_EQ(3, GetRegistryLengthFromURL("http://a.baz.jp./file.html", false));
                                                                        // 1
  EXPECT_EQ(0, GetRegistryLengthFromURL("http://ac.jp", false));        // 2
  EXPECT_EQ(0, GetRegistryLengthFromURL("http://a.bar.jp", false));     // 3
  EXPECT_EQ(0, GetRegistryLengthFromURL("http://bar.jp", false));       // 3
  EXPECT_EQ(0, GetRegistryLengthFromURL("http://baz.bar.jp", false));   // 3 4
  EXPECT_EQ(12, GetRegistryLengthFromURL("http://a.b.baz.bar.jp", false));
                                                                        // 4
  EXPECT_EQ(6, GetRegistryLengthFromURL("http://foo.bar.jp", false));   // 3 5 6
  EXPECT_EQ(6, GetRegistryLengthFromURL("http://baz.pref.bar.jp", false));
                                                                        // 7
  EXPECT_EQ(11, GetRegistryLengthFromURL("http://a.b.bar.baz.com", false));
                                                                        // 8
  EXPECT_EQ(3, GetRegistryLengthFromURL("http://a.d.c", false));        // 9
  EXPECT_EQ(3, GetRegistryLengthFromURL("http://.a.d.c", false));       // 9
  EXPECT_EQ(3, GetRegistryLengthFromURL("http://..a.d.c", false));      // 9
  EXPECT_EQ(1, GetRegistryLengthFromURL("http://a.b.c", false));        // 9 10
  EXPECT_EQ(0, GetRegistryLengthFromURL("http://baz.com", false));      // none
  EXPECT_EQ(0, GetRegistryLengthFromURL("http://baz.com.", false));     // none
  EXPECT_EQ(3, GetRegistryLengthFromURL("http://baz.com", true));       // none
  EXPECT_EQ(4, GetRegistryLengthFromURL("http://baz.com.", true));      // none

  EXPECT_EQ(std::string::npos, GetRegistryLengthFromURL("", false));
  EXPECT_EQ(std::string::npos, GetRegistryLengthFromURL("http://", false));
  EXPECT_EQ(std::string::npos,
            GetRegistryLengthFromURL("file:///C:/file.html", false));
  EXPECT_EQ(0, GetRegistryLengthFromURL("http://foo.com..", false));
  EXPECT_EQ(0, GetRegistryLengthFromURL("http://...", false));
  EXPECT_EQ(0, GetRegistryLengthFromURL("http://192.168.0.1", false));
  EXPECT_EQ(0, GetRegistryLengthFromURL("http://localhost", false));
  EXPECT_EQ(0, GetRegistryLengthFromURL("http://localhost", true));
  EXPECT_EQ(0, GetRegistryLengthFromURL("http://localhost.", false));
  EXPECT_EQ(0, GetRegistryLengthFromURL("http://localhost.", true));
  EXPECT_EQ(0, GetRegistryLengthFromURL("http:////Comment", false));

  // Test std::wstring version of GetRegistryLength().  Uses the same
  // underpinnings as the GURL version, so this is really more of a check of
  // CanonicalizeHost().
  EXPECT_EQ(2, GetRegistryLengthFromHost(L"a.baz.jp", false));          // 1
  EXPECT_EQ(3, GetRegistryLengthFromHost(L"a.baz.jp.", false));         // 1
  EXPECT_EQ(0, GetRegistryLengthFromHost(L"ac.jp", false));             // 2
  EXPECT_EQ(0, GetRegistryLengthFromHost(L"a.bar.jp", false));          // 3
  EXPECT_EQ(0, GetRegistryLengthFromHost(L"bar.jp", false));            // 3
  EXPECT_EQ(0, GetRegistryLengthFromHost(L"baz.bar.jp", false));        // 3 4
  EXPECT_EQ(12, GetRegistryLengthFromHost(L"a.b.baz.bar.jp", false));   // 4
  EXPECT_EQ(6, GetRegistryLengthFromHost(L"foo.bar.jp", false));        // 3 5 6
  EXPECT_EQ(6, GetRegistryLengthFromHost(L"baz.pref.bar.jp", false));   // 7
  EXPECT_EQ(11, GetRegistryLengthFromHost(L"a.b.bar.baz.com", false));  // 8
  EXPECT_EQ(3, GetRegistryLengthFromHost(L"a.d.c", false));             // 9
  EXPECT_EQ(3, GetRegistryLengthFromHost(L".a.d.c", false));            // 9
  EXPECT_EQ(3, GetRegistryLengthFromHost(L"..a.d.c", false));           // 9
  EXPECT_EQ(1, GetRegistryLengthFromHost(L"a.b.c", false));             // 9 10
  EXPECT_EQ(0, GetRegistryLengthFromHost(L"baz.com", false));           // none
  EXPECT_EQ(0, GetRegistryLengthFromHost(L"baz.com.", false));          // none
  EXPECT_EQ(3, GetRegistryLengthFromHost(L"baz.com", true));            // none
  EXPECT_EQ(4, GetRegistryLengthFromHost(L"baz.com.", true));           // none

  EXPECT_EQ(std::string::npos, GetRegistryLengthFromHost(L"", false));
  EXPECT_EQ(0, GetRegistryLengthFromHost(L"foo.com..", false));
  EXPECT_EQ(0, GetRegistryLengthFromHost(L"..", false));
  EXPECT_EQ(0, GetRegistryLengthFromHost(L"192.168.0.1", false));
  EXPECT_EQ(0, GetRegistryLengthFromHost(L"localhost", false));
  EXPECT_EQ(0, GetRegistryLengthFromHost(L"localhost", true));
  EXPECT_EQ(0, GetRegistryLengthFromHost(L"localhost.", false));
  EXPECT_EQ(0, GetRegistryLengthFromHost(L"localhost.", true));
}

TEST_F(RegistryControlledDomainTest, TestSameDomainOrHost) {
  SetTestData("jp\nbar.jp");

  EXPECT_EQ(true, CompareDomains("http://a.b.bar.jp/file.html",
                                 "http://a.b.bar.jp/file.html")); // b.bar.jp
  EXPECT_EQ(true, CompareDomains("http://a.b.bar.jp/file.html",
                                 "http://b.b.bar.jp/file.html")); // b.bar.jp
  EXPECT_EQ(false, CompareDomains("http://a.foo.jp/file.html",    // foo.jp
                                  "http://a.not.jp/file.html"));  // not.jp
  EXPECT_EQ(false, CompareDomains("http://a.foo.jp/file.html",    // foo.jp
                                  "http://a.foo.jp./file.html")); // foo.jp.
  EXPECT_EQ(false, CompareDomains("http://a.com/file.html",       // a.com
                                  "http://b.com/file.html"));     // b.com
  EXPECT_EQ(true, CompareDomains("http://a.x.com/file.html",
                                 "http://b.x.com/file.html"));    // x.com
  EXPECT_EQ(true, CompareDomains("http://a.x.com/file.html",
                                 "http://.x.com/file.html"));     // x.com
  EXPECT_EQ(true, CompareDomains("http://a.x.com/file.html",
                                 "http://..b.x.com/file.html"));  // x.com
  EXPECT_EQ(true, CompareDomains("http://intranet/file.html",
                                 "http://intranet/file.html"));   // intranet
  EXPECT_EQ(true, CompareDomains("http://127.0.0.1/file.html",
                                 "http://127.0.0.1/file.html"));  // 127.0.0.1
  EXPECT_EQ(false, CompareDomains("http://192.168.0.1/file.html", // 192.168.0.1
                                  "http://127.0.0.1/file.html")); // 127.0.0.1
  EXPECT_EQ(false, CompareDomains("file:///C:/file.html",
                                  "file:///C:/file.html"));       // no host
}

TEST_F(RegistryControlledDomainTest, TestDefaultData) {
  // Note that no data is set: we're using the default rules.
  EXPECT_EQ(3, GetRegistryLengthFromURL("http://google.com", false));
  EXPECT_EQ(3, GetRegistryLengthFromURL("http://stanford.edu", false));
  EXPECT_EQ(3, GetRegistryLengthFromURL("http://ustreas.gov", false));
  EXPECT_EQ(3, GetRegistryLengthFromURL("http://icann.net", false));
  EXPECT_EQ(3, GetRegistryLengthFromURL("http://ferretcentral.org", false));
  EXPECT_EQ(0, GetRegistryLengthFromURL("http://nowhere.foo", false));
  EXPECT_EQ(3, GetRegistryLengthFromURL("http://nowhere.foo", true));
}
