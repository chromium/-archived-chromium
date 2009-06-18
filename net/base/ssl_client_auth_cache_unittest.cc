// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_client_auth_cache.h"

#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(SSLClientAuthCacheTest, LookupAddRemove) {
  SSLClientAuthCache cache;

  base::Time start_date = base::Time::Now();
  base::Time expiration_date = start_date + base::TimeDelta::FromDays(1);

  std::string server1("foo1:443");
  scoped_refptr<X509Certificate> cert1(
      new X509Certificate("foo1", "CA", start_date, expiration_date));

  std::string server2("foo2:443");
  scoped_refptr<X509Certificate> cert2(
      new X509Certificate("foo2", "CA", start_date, expiration_date));

  std::string server3("foo3:443");
  scoped_refptr<X509Certificate> cert3(
    new X509Certificate("foo3", "CA", start_date, expiration_date));

  // Lookup non-existent client certificate.
  EXPECT_EQ(NULL, cache.Lookup(server1));

  // Add client certificate for server1.
  cache.Add(server1, cert1.get());
  EXPECT_EQ(cert1.get(), cache.Lookup(server1));

  // Add client certificate for server2.
  cache.Add(server2, cert2.get());
  EXPECT_EQ(cert1.get(), cache.Lookup(server1));
  EXPECT_EQ(cert2.get(), cache.Lookup(server2));

  // Overwrite the client certificate for server1.
  cache.Add(server1, cert3.get());
  EXPECT_EQ(cert3.get(), cache.Lookup(server1));
  EXPECT_EQ(cert2.get(), cache.Lookup(server2));

  // Remove client certificate of server1.
  cache.Remove(server1);
  EXPECT_EQ(NULL, cache.Lookup(server1));
  EXPECT_EQ(cert2.get(), cache.Lookup(server2));

  // Remove non-existent client certificate.
  cache.Remove(server1);
  EXPECT_EQ(NULL, cache.Lookup(server1));
  EXPECT_EQ(cert2.get(), cache.Lookup(server2));
}

// Check that if the server differs only by port number, it is considered
// a separate server.
TEST(SSLClientAuthCacheTest, LookupWithPort) {
  SSLClientAuthCache cache;

  base::Time start_date = base::Time::Now();
  base::Time expiration_date = start_date + base::TimeDelta::FromDays(1);

  std::string server1("foo:443");
  scoped_refptr<X509Certificate> cert1(
      new X509Certificate("foo", "CA", start_date, expiration_date));

  std::string server2("foo:8443");
  scoped_refptr<X509Certificate> cert2(
      new X509Certificate("foo", "CA", start_date, expiration_date));

  cache.Add(server1, cert1.get());
  cache.Add(server2, cert2.get());

  EXPECT_EQ(cert1.get(), cache.Lookup(server1));
  EXPECT_EQ(cert2.get(), cache.Lookup(server2));
}

}  // namespace net
