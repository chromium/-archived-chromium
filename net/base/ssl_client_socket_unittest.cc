// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "net/base/address_list.h"
#include "net/base/client_socket_factory.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_unittest.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_client_socket.h"
#include "net/base/ssl_config_service.h"
#include "net/base/tcp_client_socket.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

const net::SSLConfig kDefaultSSLConfig;

class SSLClientSocketTest : public PlatformTest {
 public:
  SSLClientSocketTest()
      : host_mapper_(new net::RuleBasedHostMapper()),
        scoped_host_mapper_(host_mapper_.get()),
        socket_factory_(net::ClientSocketFactory::GetDefaultFactory()) {
    // TODO(darin): kill this exception once we have a way to test out the
    // TCPClientSocket class using loopback connections.
    host_mapper_->AddRule("bugs.webkit.org", "bugs.webkit.org");
  }

 protected:
  scoped_refptr<net::RuleBasedHostMapper> host_mapper_;
  net::ScopedHostMapper scoped_host_mapper_;
  net::ClientSocketFactory* socket_factory_;
};

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
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(new net::TCPClientSocket(addr),
                                             hostname, kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);
  }
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  rv = sock->Write(request_text, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);
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
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(new net::TCPClientSocket(addr),
                                             hostname, kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  rv = sock->Write(request_text, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);
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
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(new net::TCPClientSocket(addr),
                                             hostname, kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  rv = sock->Write(request_text, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);
  }

  // Do a partial read and then exit.  This test should not crash!
  char buf[512];
  rv = sock->Read(buf, sizeof(buf), &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_NE(rv, 0);
}

