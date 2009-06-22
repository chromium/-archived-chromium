// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_client_socket.h"

#include "base/basictypes.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/listen_socket.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/winsock_init.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

const char kServerReply[] = "HTTP/1.1 404 Not Found";

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
    connected_sock_->Send(kServerReply,
                          arraysize(kServerReply) - 1,
                          false /* don't append line feed */);
  }
  virtual void DidClose(ListenSocket* sock) {}

  // Testcase hooks
  virtual void SetUp();

  void CloseServerSocket() {
    // delete the connected_sock_, which will close it.
    connected_sock_ = NULL;
  }

  void PauseServerReads() {
    connected_sock_->PauseReads();
  }

  void ResumeServerReads() {
    connected_sock_->ResumeReads();
  }

 protected:
  int listen_port_;
  scoped_ptr<TCPClientSocket> sock_;

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
  EnsureWinsockInit();
#endif
  for (port = kMinPort; port < kMaxPort; port++) {
    sock = ListenSocket::Listen("127.0.0.1", port, this);
    if (sock)
      break;
  }
  ASSERT_TRUE(sock != NULL);
  listen_sock_ = sock;
  listen_port_ = port;

  AddressList addr;
  HostResolver resolver;
  HostResolver::RequestInfo info("localhost", listen_port_);
  int rv = resolver.Resolve(info, &addr, NULL, NULL);
  CHECK(rv == OK);
  sock_.reset(new TCPClientSocket(addr));
}

TEST_F(TCPClientSocketTest, Connect) {
  TestCompletionCallback callback;
  EXPECT_FALSE(sock_->IsConnected());

  int rv = sock_->Connect(&callback);
  if (rv != OK) {
    ASSERT_EQ(rv, ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, OK);
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
  if (rv != OK) {
    ASSERT_EQ(rv, ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, OK);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<IOBuffer> request_buffer =
      new IOBuffer(arraysize(request_text) - 1);
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock_->Write(request_buffer, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, static_cast<int>(arraysize(request_text) - 1));
  }

  scoped_refptr<IOBuffer> buf = new IOBuffer(4096);
  uint32 bytes_read = 0;
  while (bytes_read < arraysize(kServerReply) - 1) {
    rv = sock_->Read(buf, 4096, &callback);
    EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();

    ASSERT_GE(rv, 0);
    bytes_read += rv;
  }

  // All data has been read now.  Read once more to force an ERR_IO_PENDING, and
  // then close the server socket, and note the close.

  rv = sock_->Read(buf, 4096, &callback);
  ASSERT_EQ(ERR_IO_PENDING, rv);
  CloseServerSocket();
  EXPECT_EQ(0, callback.WaitForResult());
}

TEST_F(TCPClientSocketTest, Read_SmallChunks) {
  TestCompletionCallback callback;
  int rv = sock_->Connect(&callback);
  if (rv != OK) {
    ASSERT_EQ(rv, ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, OK);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<IOBuffer> request_buffer =
      new IOBuffer(arraysize(request_text) - 1);
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock_->Write(request_buffer, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, static_cast<int>(arraysize(request_text) - 1));
  }

  scoped_refptr<IOBuffer> buf = new IOBuffer(1);
  uint32 bytes_read = 0;
  while (bytes_read < arraysize(kServerReply) - 1) {
    rv = sock_->Read(buf, 1, &callback);
    EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();

    ASSERT_EQ(1, rv);
    bytes_read += rv;
  }

  // All data has been read now.  Read once more to force an ERR_IO_PENDING, and
  // then close the server socket, and note the close.

  rv = sock_->Read(buf, 1, &callback);
  ASSERT_EQ(ERR_IO_PENDING, rv);
  CloseServerSocket();
  EXPECT_EQ(0, callback.WaitForResult());
}

TEST_F(TCPClientSocketTest, Read_Interrupted) {
  TestCompletionCallback callback;
  int rv = sock_->Connect(&callback);
  if (rv != OK) {
    ASSERT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, OK);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<IOBuffer> request_buffer =
      new IOBuffer(arraysize(request_text) - 1);
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock_->Write(request_buffer, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, static_cast<int>(arraysize(request_text) - 1));
  }

  // Do a partial read and then exit.  This test should not crash!
  scoped_refptr<IOBuffer> buf = new IOBuffer(16);
  rv = sock_->Read(buf, 16, &callback);
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_NE(0, rv);
}

TEST_F(TCPClientSocketTest, DISABLED_FullDuplex_ReadFirst) {
  TestCompletionCallback callback;
  int rv = sock_->Connect(&callback);
  if (rv != OK) {
    ASSERT_EQ(rv, ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, OK);
  }

  // Read first.  There's no data, so it should return ERR_IO_PENDING.
  const int kBufLen = 4096;
  scoped_refptr<IOBuffer> buf = new IOBuffer(kBufLen);
  rv = sock_->Read(buf, kBufLen, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  PauseServerReads();
  const int kWriteBufLen = 64 * 1024;
  scoped_refptr<IOBuffer> request_buffer = new IOBuffer(kWriteBufLen);
  char* request_data = request_buffer->data();
  memset(request_data, 'A', kWriteBufLen);
  TestCompletionCallback write_callback;

  while (true) {
    rv = sock_->Write(request_buffer, kWriteBufLen, &write_callback);
    ASSERT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING) {
      ResumeServerReads();
      rv = write_callback.WaitForResult();
      break;
    }
  }

  // At this point, both read and write have returned ERR_IO_PENDING, and the
  // write callback has executed.  We wait for the read callback to run now to
  // make sure that the socket can handle full duplex communications.

  rv = callback.WaitForResult();
  EXPECT_GE(rv, 0);
}

TEST_F(TCPClientSocketTest, DISABLED_FullDuplex_WriteFirst) {
  TestCompletionCallback callback;
  int rv = sock_->Connect(&callback);
  if (rv != OK) {
    ASSERT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(OK, rv);
  }

  PauseServerReads();
  const int kWriteBufLen = 64 * 1024;
  scoped_refptr<IOBuffer> request_buffer = new IOBuffer(kWriteBufLen);
  char* request_data = request_buffer->data();
  memset(request_data, 'A', kWriteBufLen);
  TestCompletionCallback write_callback;

  while (true) {
    rv = sock_->Write(request_buffer, kWriteBufLen, &write_callback);
    ASSERT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING)
      break;
  }

  // Now we have the Write() blocked on ERR_IO_PENDING.  It's time to force the
  // Read() to block on ERR_IO_PENDING too.

  const int kBufLen = 4096;
  scoped_refptr<IOBuffer> buf = new IOBuffer(kBufLen);
  while (true) {
    rv = sock_->Read(buf, kBufLen, &callback);
    ASSERT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);
    if (rv == ERR_IO_PENDING)
      break;
  }

  // At this point, both read and write have returned ERR_IO_PENDING.  Now we
  // run the write and read callbacks to make sure they can handle full duplex
  // communications.

  ResumeServerReads();
  rv = write_callback.WaitForResult();
  EXPECT_GE(rv, 0);

  // It's possible the read is blocked because it's already read all the data.
  // Close the server socket, so there will at least be a 0-byte read.
  CloseServerSocket();

  rv = callback.WaitForResult();
  EXPECT_GE(rv, 0);
}

}  // namespace

}  // namespace net
