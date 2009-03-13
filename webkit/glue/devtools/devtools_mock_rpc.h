// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_MOCK_RPC_H_
#define WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_MOCK_RPC_H_

#include <string>

#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/devtools/devtools_rpc.h"
#include "webkit/glue/glue_util.h"

// Universal mock delegate for DevToolsRpc. Typical usage of the mock is:
// mock->Method1();  // Set expectation.
// mock->Replay();
// // Do Something here;
// mock->Verify();  // Verify.
class DevToolsMockRpc : public DevToolsRpc::Delegate {
 public:
  DevToolsMockRpc() {}
  ~DevToolsMockRpc() {}

  virtual void SendRpcMessage(const std::string& msg) {
    if (!log_.length()) {
      log_ = msg;
    } else {
      log_ = StringPrintf("%s\n%s", log_.c_str(), msg.c_str());
    }
  }

  void Replay() {
    ref_log_ = log_;
    log_ = "";
  }

  void Verify() {
    ASSERT_EQ(ref_log_, log_);
  }

  void Reset() {
    ref_log_ = "";
    log_ = "";
  }

  const std::string &get_log() { return log_; }

 private:
  std::string log_;
  std::string ref_log_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsMockRpc);
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_MOCK_RPC_H_
