// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/listen_socket.h"
#include "net/base/net_errors.h"
#include "net/base/tcp_client_socket.h"
#include "net/base/test_completion_callback.h"
#include "net/base/winsock_init.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class TCPClientSocketTest
    : public PlatformTest, public ListenSocket::ListenSocketDelegate {
 public:
  TCPClientSocketTest() {
  }

  // Implement ListenSocketDelegate methods
  virtual void DidAccept(ListenSocket* server, ListenSocket* connection) {
    connected_sock_ = connection;
  }
  virtual void DidRead(ListenSocket*, const std::string& s) {
    // TODO(dkegel): this might not be long enough to tickle some bugs.
    connected_sock_->Send(std::string("HTTP/1.1 404 Not Found"), true);
    // Close socket by destroying it, else read test below will hang.
    connected_sock_ = NULL;
  }
  virtual void DidClose(ListenSocket* sock) {}

  // Testcase hooks
  virtual void SetUp();

 protected:
  int listen_port_;

 private:
  scoped_refptr<ListenSocket> listen_sock_;
  scoped_refptr<ListenSocket> connected_sock_;
};

void TCPClientSocketTest::SetUp() {
  PlatformTest::SetUp();

  // Find a free port to listen on
  ListenSocket *sock = NULL;
  int port;
  // Range of ports to listen on.  Shouldn't need to try many.
  static const int kMinPort = 10100;
  static const int kMaxPort = 10200;
#if defined(OS_WIN)
  net::EnsureWinsockInit();
#endif
  for (port = kMinPort; port < kMaxPort; port++) {
    sock = ListenSocket::Listen("127.0.0.1", port, this);
    if (sock)
        break;
  }
  ASSERT_TRUE(sock != NULL);
  listen_sock_ = sock;
  listen_port_ = port;
}

TEST_F(TCPClientSocketTest, Connect) {
  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  int rv = resolver.Resolve("localhost", listen_port_, &addr, NULL);
  EXPECT_EQ(rv, net::OK);

  net::TCPClientSocket sock(addr);

  EXPECT_FALSE(sock.IsConnected());

  rv = sock.Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(rv, net::ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, net::OK);
  }

  EXPECT_TRUE(sock.IsConnected());

  sock.Disconnect();
  EXPECT_FALSE(sock.IsConnected());
}

TEST_F(TCPClientSocketTest, Read) {
  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  int rv = resolver.Resolve("localhost", listen_port_, &addr, &callback);
  EXPECT_EQ(rv, net::ERR_IO_PENDING);

  rv = callback.WaitForResult();
  EXPECT_EQ(rv, net::OK);

  net::TCPClientSocket sock(addr);

  rv = sock.Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(rv, net::ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, net::OK);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  rv = sock.Write(request_text, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, static_cast<int>(arraysize(request_text) - 1));
  }

  char buf[4096];
  for (;;) {
    rv = sock.Read(buf, sizeof(buf), &callback);
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

TEST_F(TCPClientSocketTest, Read_SmallChunks) {
  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  int rv = resolver.Resolve("localhost", listen_port_, &addr, NULL);
  EXPECT_EQ(rv, net::OK);

  net::TCPClientSocket sock(addr);

  rv = sock.Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(rv, net::ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, net::OK);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  rv = sock.Write(request_text, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, static_cast<int>(arraysize(request_text) - 1));
  }

  char buf[1];
  for (;;) {
    rv = sock.Read(buf, sizeof(buf), &callback);
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

TEST_F(TCPClientSocketTest, Read_Interrupted) {
  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  int rv = resolver.Resolve("localhost", listen_port_, &addr, NULL);
  EXPECT_EQ(rv, net::OK);

  net::TCPClientSocket sock(addr);

  rv = sock.Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(rv, net::ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, net::OK);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  rv = sock.Write(request_text, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, static_cast<int>(arraysize(request_text) - 1));
  }

  // Do a partial read and then exit.  This test should not crash!
  char buf[512];
  rv = sock.Read(buf, sizeof(buf), &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_NE(rv, 0);
}
