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

  virtual void SendRpcMessage(const std::string& class_name,
                              const std::string& method_name,
                              const std::string& msg) {
    last_class_name_ = class_name;
    last_method_name_ = method_name;
    last_msg_ = msg;

    if (!log_.length()) {
      log_ = StringPrintf("%s %s %s", class_name.c_str(),
                          method_name.c_str(), msg.c_str());;
    } else {
      log_ = StringPrintf("%s\n%s %s %s", log_.c_str(), class_name.c_str(),
                          method_name.c_str(), msg.c_str());
    }
  }

  void Replay() {
    ref_last_class_name_ = last_class_name_;
    last_class_name_ = "";
    ref_last_method_name_ = last_method_name_;
    last_method_name_ = "";
    ref_last_msg_ = last_msg_;
    last_msg_ = "";

    ref_log_ = log_;
    log_ = "";
  }

  void Verify() {
    ASSERT_EQ(ref_last_class_name_, last_class_name_);
    ASSERT_EQ(ref_last_method_name_, last_method_name_);
    ASSERT_EQ(ref_last_msg_, last_msg_);
    ASSERT_EQ(ref_log_, log_);
  }

  void Reset() {
    last_class_name_ = "";
    last_method_name_ = "";
    last_msg_ = "";

    ref_last_class_name_ = "";
    ref_last_method_name_ = "";
    ref_last_msg_ = "";

    ref_log_ = "";
    log_ = "";
  }

  const std::string& last_class_name() { return last_class_name_; }
  const std::string& last_method_name() { return last_method_name_; }
  const std::string& last_msg() { return last_msg_; }

 private:
  std::string last_class_name_;
  std::string last_method_name_;
  std::string last_msg_;

  std::string ref_last_class_name_;
  std::string ref_last_method_name_;
  std::string ref_last_msg_;

  std::string log_;
  std::string ref_log_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsMockRpc);
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_MOCK_RPC_H_
