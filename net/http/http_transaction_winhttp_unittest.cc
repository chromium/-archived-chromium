// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_transaction_winhttp.h"
#include "net/http/http_transaction_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(HttpTransactionWinHttp, CreateAndDestroy) {
  scoped_ptr<net::ProxyService> proxy_service(net::ProxyService::CreateNull());
  net::HttpTransactionWinHttp::Factory factory(proxy_service.get());

  scoped_ptr<net::HttpTransaction> trans(factory.CreateTransaction());
}

TEST(HttpTransactionWinHttp, Suspend) {
  scoped_ptr<net::ProxyService> proxy_service(net::ProxyService::CreateNull());
  net::HttpTransactionWinHttp::Factory factory(proxy_service.get());

  scoped_ptr<net::HttpTransaction> trans(factory.CreateTransaction());
  trans.reset();

  factory.Suspend(true);

  trans.reset(factory.CreateTransaction());
  ASSERT_TRUE(trans == NULL);

  factory.Suspend(false);

  trans.reset(factory.CreateTransaction());
}

TEST(HttpTransactionWinHttp, GoogleGET) {
  scoped_ptr<net::ProxyService> proxy_service(net::ProxyService::CreateNull());
  net::HttpTransactionWinHttp::Factory factory(proxy_service.get());
  TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans(factory.CreateTransaction());

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
  rv = ReadTransaction(trans.get(), &contents);
  EXPECT_EQ(net::OK, rv);
}

