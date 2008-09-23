// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_test.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/tcp_client_socket.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

class TCPClientSocketTest : public PlatformTest {
};


TEST_F(TCPClientSocketTest, Connect) {
  net::AddressList addr;
  net::HostResolver resolver;
  TestCompletionCallback callback;

  int rv = resolver.Resolve("www.google.com", 80, &addr, NULL);
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

  int rv = resolver.Resolve("www.google.com", 80, &addr, &callback);
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

  int rv = resolver.Resolve("www.google.com", 80, &addr, NULL);
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

  int rv = resolver.Resolve("www.google.com", 80, &addr, NULL);
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
