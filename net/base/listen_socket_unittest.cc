// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests ListenSocket.

#include "net/base/listen_socket_unittest.h"

namespace {

class ListenSocketTest: public testing::Test {
public:
  ListenSocketTest() {
    tester_ = NULL;
  }

  virtual void SetUp() {
    tester_ = new ListenSocketTester();
    tester_->SetUp();
  }

  virtual void TearDown() {
    tester_->TearDown();
    tester_ = NULL;
  }

  scoped_refptr<ListenSocketTester> tester_;
};

}  // namespace

TEST_F(ListenSocketTest, ClientSend) {
  tester_->TestClientSend();
}

TEST_F(ListenSocketTest, ClientSendLong) {
  tester_->TestClientSendLong();
}

TEST_F(ListenSocketTest, ServerSend) {
  tester_->TestServerSend();
}

