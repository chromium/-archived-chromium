// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_test.h"
#include "net/base/scoped_host_mapper.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_transaction_unittest.h"
#include "net/proxy/proxy_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class HttpNetworkLayerTest : public PlatformTest {
 public:
  HttpNetworkLayerTest() {
    // TODO(darin): kill this exception once we have a way to test out the
    // HttpNetworkLayer class using loopback connections.
    host_mapper_.AddRule("www.google.com", "www.google.com");
  }
 private:
  net::ScopedHostMapper host_mapper_;
};

TEST_F(HttpNetworkLayerTest, CreateAndDestroy) {
  net::HttpNetworkLayer factory(NULL);

  net::HttpTransaction* trans = factory.CreateTransaction();
  trans->Destroy();
}

TEST_F(HttpNetworkLayerTest, Suspend) {
  net::HttpNetworkLayer factory(NULL);

  net::HttpTransaction* trans = factory.CreateTransaction();
  trans->Destroy();

  factory.Suspend(true);

  trans = factory.CreateTransaction();
  ASSERT_TRUE(trans == NULL);

  factory.Suspend(false);

  trans = factory.CreateTransaction();
  trans->Destroy();
}

TEST_F(HttpNetworkLayerTest, GoogleGET) {
  net::ProxyInfo no_proxy;  // Avoid using a proxy server.
  net::HttpNetworkLayer factory(&no_proxy);

  TestCompletionCallback callback;

  net::HttpTransaction* trans = factory.CreateTransaction();

  net::HttpRequestInfo request_info;
  request_info.url = GURL("http://www.google.com/");
  request_info.method = "GET";
  request_info.user_agent = "Foo/1.0";
  request_info.load_flags = net::LOAD_NORMAL;

  int rv = trans->Start(&request_info, &callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  std::string contents;
  rv = ReadTransaction(trans, &contents);
  EXPECT_EQ(net::OK, rv);

  trans->Destroy();
}

