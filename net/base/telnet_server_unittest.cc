// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests TelnetServer.

#include "net/base/listen_socket_unittest.h"
#include "net/base/telnet_server.h"
#include "testing/platform_test.h"

static const char* kCRLF = "\r\n";

class TelnetServerTester : public ListenSocketTester {
public:
  virtual ListenSocket* DoListen() {
    return TelnetServer::Listen("127.0.0.1", kTestPort, this);
  }

  virtual void SetUp() {
    ListenSocketTester::SetUp();
    // With TelnetServer, there's some control codes sent at connect time,
    // so we need to eat those to avoid affecting the subsequent tests.
    EXPECT_EQ(ClearTestSocket(), 15);
  }

  virtual bool Send(SOCKET sock, const std::string& str) {
    if (ListenSocketTester::Send(sock, str)) {
      // TelnetServer currently calls DidRead after a CRLF, so we need to
      // append one to the end of the data that we send.
      if (ListenSocketTester::Send(sock, kCRLF)) {
        return true;
      }
    }
    return false;
  }
};

class TelnetServerTest: public PlatformTest {
protected:
  TelnetServerTest() {
    tester_ = NULL;
  }

  virtual void SetUp() {
    PlatformTest::SetUp();
    tester_ = new TelnetServerTester();
    tester_->SetUp();
  }

  virtual void TearDown() {
    PlatformTest::TearDown();
    tester_->TearDown();
    tester_ = NULL;
  }

  scoped_refptr<TelnetServerTester> tester_;
};

TEST_F(TelnetServerTest, ServerClientSend) {
  tester_->TestClientSend();
}

TEST_F(TelnetServerTest, ClientSendLong) {
  tester_->TestClientSendLong();
}

TEST_F(TelnetServerTest, ServerSend) {
  tester_->TestServerSend();
}
