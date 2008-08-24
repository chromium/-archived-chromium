// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests TelnetServer.

#include "net/base/listen_socket_unittest.h"
#include "net/base/telnet_server.h"

namespace {

const std::string CRLF("\r\n");

class TelnetServerTester : public ListenSocketTester {
public:
  virtual ListenSocket* DoListen() {
    return TelnetServer::Listen("127.0.0.1", TEST_PORT, this);
  }

  virtual void SetUp() {
    ListenSocketTester::SetUp();
    // With TelnetServer, there's some control codes sent at connect time,
    // so we need to eat those to avoid affecting the subsequent tests.
    // TODO(erikkay): Unfortunately, without the sleep, we don't seem to
    // reliably get the 15 bytes without an EWOULDBLOCK.  It would be nice if
    // there were a more reliable mechanism here.
    Sleep(10);
    ASSERT_EQ(ClearTestSocket(), 15);
  }

  virtual bool Send(SOCKET sock, const std::string& str) {
    if (ListenSocketTester::Send(sock, str)) {
      // TelnetServer currently calls DidRead after a CRLF, so we need to
      // append one to the end of the data that we send.
      if (ListenSocketTester::Send(sock, CRLF)) {
        return true;
      }
    }
    return false;
  }
};

class TelnetServerTest: public testing::Test {
protected:
  TelnetServerTest() {
    tester_ = NULL;
  }

  virtual void SetUp() {
    tester_ = new TelnetServerTester();
    tester_->SetUp();
  }

  virtual void TearDown() {
    tester_->TearDown();
    tester_ = NULL;
  }

  scoped_refptr<TelnetServerTester> tester_;
};

}  // namespace

TEST_F(TelnetServerTest, ServerClientSend) {
  tester_->TestClientSend();
}

TEST_F(TelnetServerTest, ClientSendLong) {
  tester_->TestClientSendLong();
}

TEST_F(TelnetServerTest, ServerSend) {
  tester_->TestServerSend();
}

