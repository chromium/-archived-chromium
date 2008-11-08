// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_cache.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class MockAuthHandler : public HttpAuthHandler {
 public:
  MockAuthHandler(const char* scheme, const char* realm,
                  HttpAuth::Target target) {
    // Can't use initializer list since these are members of the base class.
    scheme_ = scheme;
    realm_ = realm;
    score_ = 1;
    target_ = target;
  }
  
  virtual std::string GenerateCredentials(const std::wstring&,
                                          const std::wstring&,
                                          const HttpRequestInfo*,
                                          const ProxyInfo*) {
    return "mock-credentials";  // Unused.
  }

 protected:
  virtual bool Init(std::string::const_iterator, std::string::const_iterator) {
    return false;  // Unused.
  }
};

} // namespace

// Test adding and looking-up cache entries (both by realm and by path).
TEST(HttpAuthCacheTest, Basic) {
  GURL origin("http://www.google.com");
  HttpAuthCache cache;
  HttpAuthCache::Entry* entry;
  
  // Add cache entries for 3 realms: "Realm1", "Realm2", "Realm3"

  scoped_refptr<HttpAuthHandler> realm1_handler =
      new MockAuthHandler("basic", "Realm1", HttpAuth::AUTH_SERVER);
  cache.Add(origin, realm1_handler, L"realm1-user", L"realm1-password",
      "/foo/bar/index.html");

  scoped_refptr<HttpAuthHandler> realm2_handler =
      new MockAuthHandler("basic", "Realm2", HttpAuth::AUTH_SERVER);
  cache.Add(origin, realm2_handler, L"realm2-user", L"realm2-password",
      "/foo2/index.html");

  scoped_refptr<HttpAuthHandler> realm3_handler =
      new MockAuthHandler("basic", "Realm3", HttpAuth::AUTH_PROXY);
  cache.Add(origin, realm3_handler, L"realm3-user", L"realm3-password", "");

  // There is no Realm4
  entry = cache.LookupByRealm(origin, "Realm4");
  EXPECT_TRUE(NULL == entry);

  // While Realm3 does exist, the origin scheme is wrong.
  entry = cache.LookupByRealm(GURL("https://www.google.com"), "Realm3");
  EXPECT_TRUE(NULL == entry);

  // Valid lookup by realm.
  entry = cache.LookupByRealm(GURL("http://www.google.com:80"), "Realm3");
  EXPECT_FALSE(NULL == entry);
  EXPECT_TRUE(entry->handler() == realm3_handler.get());
  EXPECT_EQ(L"realm3-user", entry->username());
  EXPECT_EQ(L"realm3-password", entry->password());

  // Valid lookup by realm.
  entry = cache.LookupByRealm(origin, "Realm2");
  EXPECT_FALSE(NULL == entry);
  EXPECT_TRUE(entry->handler() == realm2_handler.get());
  EXPECT_EQ(L"realm2-user", entry->username());
  EXPECT_EQ(L"realm2-password", entry->password());
  
  // Check that subpaths are recognized.
  HttpAuthCache::Entry* realm2Entry = cache.LookupByRealm(origin, "Realm2");
  EXPECT_FALSE(NULL == realm2Entry);
  // Positive tests:
  entry = cache.LookupByPath(origin, "/foo2/index.html");
  EXPECT_TRUE(realm2Entry == entry);
  entry = cache.LookupByPath(origin, "/foo2/foobar.html");
  EXPECT_TRUE(realm2Entry == entry);
  entry = cache.LookupByPath(origin, "/foo2/bar/index.html");
  EXPECT_TRUE(realm2Entry == entry);
  entry = cache.LookupByPath(origin, "/foo2/");
  EXPECT_TRUE(realm2Entry == entry);
  // Negative tests:
  entry = cache.LookupByPath(origin, "/foo2");
  EXPECT_FALSE(realm2Entry == entry);
  entry = cache.LookupByPath(origin, "/foo3/index.html");
  EXPECT_FALSE(realm2Entry == entry);
  entry = cache.LookupByPath(origin, "");
  EXPECT_FALSE(realm2Entry == entry);
  entry = cache.LookupByPath(origin, "/");
  EXPECT_FALSE(realm2Entry == entry);

  // Lookup using empty path (may be used for proxy).
  entry = cache.LookupByPath(origin, "");
  EXPECT_FALSE(NULL == entry);
  EXPECT_TRUE(entry->handler() == realm3_handler.get());
  EXPECT_EQ("Realm3", entry->realm());
}

TEST(HttpAuthCacheTest, AddPath) {
  HttpAuthCache::Entry entry;

  // All of these paths have a common root /1/2/2/4/5/
  entry.AddPath("/1/2/3/4/5/x.txt");
  entry.AddPath("/1/2/3/4/5/y.txt");
  entry.AddPath("/1/2/3/4/5/z.txt");

  EXPECT_EQ(1U, entry.paths_.size());
  EXPECT_EQ("/1/2/3/4/5/", entry.paths_.front());

  // Add a new entry (not a subpath).
  entry.AddPath("/1/XXX/q");
  EXPECT_EQ(2U, entry.paths_.size());
  EXPECT_EQ("/1/XXX/", entry.paths_.front());
  EXPECT_EQ("/1/2/3/4/5/", entry.paths_.back());

  // Add containing paths of /1/2/3/4/5/ -- should swallow up the deeper paths.
  entry.AddPath("/1/2/3/4/x.txt");
  EXPECT_EQ(2U, entry.paths_.size());
  EXPECT_EQ("/1/2/3/4/", entry.paths_.front());
  EXPECT_EQ("/1/XXX/", entry.paths_.back());
  entry.AddPath("/1/2/3/x");
  EXPECT_EQ(2U, entry.paths_.size());
  EXPECT_EQ("/1/2/3/", entry.paths_.front());
  EXPECT_EQ("/1/XXX/", entry.paths_.back());

  entry.AddPath("/index.html");
  EXPECT_EQ(1U, entry.paths_.size());
  EXPECT_EQ("/", entry.paths_.front());
}

// Calling Add when the realm entry already exists, should append that
// path.
TEST(HttpAuthCacheTest, AddToExistingEntry) {
  HttpAuthCache cache;
  GURL origin("http://www.foobar.com:70");

  scoped_refptr<HttpAuthHandler> handler =
      new MockAuthHandler("basic", "MyRealm", HttpAuth::AUTH_SERVER);

  HttpAuthCache::Entry* orig_entry = cache.Add(
      origin, handler, L"user1", L"password1", "/x/y/z/");
  cache.Add(origin, handler, L"user2", L"password2", "/z/y/x/");
  cache.Add(origin, handler, L"user3", L"password3", "/z/y");

  HttpAuthCache::Entry* entry = cache.LookupByRealm(origin, "MyRealm");

  EXPECT_TRUE(entry == orig_entry);
  EXPECT_EQ(L"user3", entry->username());
  EXPECT_EQ(L"password3", entry->password());
  
  EXPECT_EQ(2U, entry->paths_.size());
  EXPECT_EQ("/z/", entry->paths_.front());
  EXPECT_EQ("/x/y/z/", entry->paths_.back());
}

TEST(HttpAuthCacheTest, Remove) {
  GURL origin("http://foobar2.com");

  scoped_refptr<HttpAuthHandler> realm1_handler =
      new MockAuthHandler("basic", "Realm1", HttpAuth::AUTH_SERVER);

  scoped_refptr<HttpAuthHandler> realm2_handler =
      new MockAuthHandler("basic", "Realm2", HttpAuth::AUTH_SERVER);

  scoped_refptr<HttpAuthHandler> realm3_handler =
      new MockAuthHandler("basic", "Realm3", HttpAuth::AUTH_SERVER);

  HttpAuthCache cache;
  cache.Add(origin, realm1_handler, L"alice", L"123", "/");
  cache.Add(origin, realm2_handler, L"bob", L"princess", "/");
  cache.Add(origin, realm3_handler, L"admin", L"password", "/");
  
  // Fails, because there is no realm "Realm4".
  EXPECT_FALSE(cache.Remove(origin, "Realm4", L"alice", L"123"));

  // Fails because the origin is wrong.
  EXPECT_FALSE(cache.Remove(
      GURL("http://foobar2.com:100"), "Realm1", L"alice", L"123"));

  // Fails because the username is wrong.
  EXPECT_FALSE(cache.Remove(origin, "Realm1", L"alice2", L"123"));

  // Fails because the password is wrong.
  EXPECT_FALSE(cache.Remove(origin, "Realm1", L"alice", L"1234"));

  // Succeeds.
  EXPECT_TRUE(cache.Remove(origin, "Realm1", L"alice", L"123"));

  // Fails because we just deleted the entry!
  EXPECT_FALSE(cache.Remove(origin, "Realm1", L"alice", L"123"));
}

} // namespace net
