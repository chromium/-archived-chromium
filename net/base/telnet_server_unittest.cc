// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
