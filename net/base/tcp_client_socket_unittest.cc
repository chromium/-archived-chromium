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
  scoped_ptr<net::TCPClientSocket> sock_;

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
  const int kMinPort = 10100;
  const int kMaxPort = 10200;
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

  net::AddressList addr;
  net::HostResolver resolver;
  int rv = resolver.Resolve("localhost", listen_port_, &addr, NULL);
  CHECK(rv == net::OK);
  sock_.reset(new net::TCPClientSocket(addr));
}

TEST_F(TCPClientSocketTest, Connect) {
  TestCompletionCallback callback;
  EXPECT_FALSE(sock_->IsConnected());

  int rv = sock_->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(rv, net::ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, net::OK);
  }

  EXPECT_TRUE(sock_->IsConnected());

  sock_->Disconnect();
  EXPECT_FALSE(sock_->IsConnected());
}

// TODO(wtc): Add unit tests for IsConnectedAndIdle:
//   - Server closes a connection.
//   - Server sends data unexpectedly.

TEST_F(TCPClientSocketTest, Read) {
  TestCompletionCallback callback;
  int rv = sock_->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(rv, net::ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, net::OK);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer =
      new net::IOBuffer(arraysize(request_text) - 1);
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock_->Write(request_buffer, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, static_cast<int>(arraysize(request_text) - 1));
  }

  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(4096);
  for (;;) {
    rv = sock_->Read(buf, 4096, &callback);
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

TEST_F(TCPClientSocketTest, Read_SmallChunks) {
  TestCompletionCallback callback;
  int rv = sock_->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(rv, net::ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, net::OK);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer =
      new net::IOBuffer(arraysize(request_text) - 1);
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock_->Write(request_buffer, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, static_cast<int>(arraysize(request_text) - 1));
  }

  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(1);
  for (;;) {
    rv = sock_->Read(buf, 1, &callback);
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

TEST_F(TCPClientSocketTest, Read_Interrupted) {
  TestCompletionCallback callback;
  int rv = sock_->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(rv, net::ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, net::OK);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer =
      new net::IOBuffer(arraysize(request_text) - 1);
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock_->Write(request_buffer, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, static_cast<int>(arraysize(request_text) - 1));
  }

  // Do a partial read and then exit.  This test should not crash!
  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(512);
  rv = sock_->Read(buf, 512, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_NE(rv, 0);
}

TEST_F(TCPClientSocketTest, FullDuplex_ReadFirst) {
  TestCompletionCallback callback;
  int rv = sock_->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(rv, net::ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, net::OK);
  }

  const int kBufLen = 4096;
  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kBufLen);
  rv = sock_->Read(buf, kBufLen, &callback);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer =
      new net::IOBuffer(arraysize(request_text) - 1);
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);
  TestCompletionCallback write_callback;

  rv = sock_->Write(request_buffer, arraysize(request_text) - 1,
                    &write_callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = write_callback.WaitForResult();
    EXPECT_EQ(rv, static_cast<int>(arraysize(request_text) - 1));
  }

  rv = callback.WaitForResult();
  EXPECT_GE(rv, 0);
  while (rv > 0) {
    rv = sock_->Read(buf, kBufLen, &callback);
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

TEST_F(TCPClientSocketTest, FullDuplex_WriteFirst) {
  TestCompletionCallback callback;
  int rv = sock_->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(rv, net::ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, net::OK);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer =
      new net::IOBuffer(arraysize(request_text) - 1);
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);
  TestCompletionCallback write_callback;

  rv = sock_->Write(request_buffer, arraysize(request_text) - 1,
                    &write_callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  const int kBufLen = 4096;
  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kBufLen);
  int read_rv = sock_->Read(buf, kBufLen, &callback);
  EXPECT_TRUE(read_rv >= 0 || read_rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING) {
    rv = write_callback.WaitForResult();
    EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);
  }

  rv = callback.WaitForResult();
  EXPECT_GE(rv, 0);
  while (rv > 0) {
    rv = sock_->Read(buf, kBufLen, &callback);
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}
