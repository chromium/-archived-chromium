// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_list.h"
#include "net/base/client_socket_factory.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_client_socket.h"
#include "net/base/ssl_config_service.h"
#include "net/base/tcp_client_socket.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

//-----------------------------------------------------------------------------

namespace {

const net::SSLConfig kDefaultSSLConfig;

class SSLClientSocketTest : public testing::Test {
 public:
  SSLClientSocketTest()
      : socket_factory_(net::ClientSocketFactory::GetDefaultFactory()) {
  }

 protected:
  net::ClientSocketFactory* socket_factory_;
};

}  // namespace

//-----------------------------------------------------------------------------

// bug 1354783
TEST_F(SSLClientSocketTest, DISABLED_Connect) {
  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  std::string hostname = "bugs.webkit.org";
  int rv = resolver.Resolve(hostname, 443, &addr, NULL);
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(new net::TCPClientSocket(addr),
                                             hostname, kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);
  }

  EXPECT_TRUE(sock->IsConnected());

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

// bug 1354783
TEST_F(SSLClientSocketTest, DISABLED_Read) {
  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  std::string hostname = "bugs.webkit.org";
  int rv = resolver.Resolve(hostname, 443, &addr, &callback);
  EXPECT_EQ(rv, net::ERR_IO_PENDING);

  rv = callback.WaitForResult();
  EXPECT_EQ(rv, net::OK);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(new net::TCPClientSocket(addr),
                                             hostname, kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(rv, net::ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, net::OK);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  rv = sock->Write(request_text, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, arraysize(request_text) - 1);
  }

  char buf[4096];
  for (;;) {
    rv = sock->Read(buf, sizeof(buf), &callback);
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

// bug 1354783
TEST_F(SSLClientSocketTest, DISABLED_Read_SmallChunks) {
  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  std::string hostname = "bugs.webkit.org";
  int rv = resolver.Resolve(hostname, 443, &addr, NULL);
  EXPECT_EQ(rv, net::OK);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(new net::TCPClientSocket(addr),
                                             hostname, kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(rv, net::ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, net::OK);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  rv = sock->Write(request_text, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, arraysize(request_text) - 1);
  }

  char buf[1];
  for (;;) {
    rv = sock->Read(buf, sizeof(buf), &callback);
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

// bug 1354783
TEST_F(SSLClientSocketTest, DISABLED_Read_Interrupted) {
  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  std::string hostname = "bugs.webkit.org";
  int rv = resolver.Resolve(hostname, 443, &addr, NULL);
  EXPECT_EQ(rv, net::OK);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(new net::TCPClientSocket(addr),
                                             hostname, kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(rv, net::ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, net::OK);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  rv = sock->Write(request_text, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, arraysize(request_text) - 1);
  }

  // Do a partial read and then exit.  This test should not crash!
  char buf[512];
  rv = sock->Read(buf, sizeof(buf), &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_NE(rv, 0);
}

