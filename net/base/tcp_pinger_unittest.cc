// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "base/trace_event.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/listen_socket.h"
#include "net/base/net_errors.h"
#include "net/base/tcp_client_socket.h"
#include "net/base/tcp_pinger.h"
#include "net/base/winsock_init.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class TCPPingerTest
    : public PlatformTest, public ListenSocket::ListenSocketDelegate {
 public:
  TCPPingerTest() {
  }

  // Implement ListenSocketDelegate methods
  virtual void DidAccept(ListenSocket* server, ListenSocket* connection) {
    // This callback doesn't seem to happen
    // right away, so this handler may not be called at all
    // during connect-only tests.
    LOG(INFO) << "TCPPinger accepted connection";
    connected_sock_ = connection;
  }
  virtual void DidRead(ListenSocket*, const std::string& s) {
    // Not really needed yet, as TCPPinger doesn't support Read
    connected_sock_->Send(std::string("HTTP/1.1 404 Not Found"), true);
    connected_sock_ = NULL;
  }
  virtual void DidClose(ListenSocket* sock) {}

  // Testcase hooks
  virtual void SetUp();

 protected:
  int listen_port_;
  scoped_refptr<ListenSocket> listen_sock_;

 private:
  scoped_refptr<ListenSocket> connected_sock_;
};

void TCPPingerTest::SetUp() {
  PlatformTest::SetUp();

  // Find a free port to listen on
  // Range of ports to listen on.  Shouldn't need to try many.
  static const int kMinPort = 10100;
  static const int kMaxPort = 10200;
#if defined(OS_WIN)
  net::EnsureWinsockInit();
#endif
  for (listen_port_ = kMinPort; listen_port_ < kMaxPort; listen_port_++) {
    listen_sock_ = ListenSocket::Listen("127.0.0.1", listen_port_, this);
    if (listen_sock_.get()) break;
  }
  ASSERT_TRUE(listen_sock_.get() != NULL);
}

TEST_F(TCPPingerTest, Ping) {
  net::AddressList addr;
  net::HostResolver resolver;

  int rv = resolver.Resolve("localhost", listen_port_, &addr, NULL);
  EXPECT_EQ(rv, net::OK);

  net::TCPPinger pinger(addr);
  rv = pinger.Ping();
  EXPECT_EQ(rv, net::OK);
}

TEST_F(TCPPingerTest, PingFail) {
  net::AddressList addr;
  net::HostResolver resolver;

  // "Kill" "server"
  listen_sock_ = NULL;

  int rv = resolver.Resolve("localhost", listen_port_, &addr, NULL);
  EXPECT_EQ(rv, net::OK);

  net::TCPPinger pinger(addr);
  rv = pinger.Ping(base::TimeDelta::FromMilliseconds(100), 1);
  EXPECT_NE(rv, net::OK);
}
