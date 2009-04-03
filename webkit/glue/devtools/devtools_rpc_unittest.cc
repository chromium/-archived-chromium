// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "PlatformString.h"
#undef LOG

#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/devtools/devtools_mock_rpc.h"
#include "webkit/glue/devtools/devtools_rpc.h"

namespace {

#define TEST_RPC_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3, METHOD4) \
  METHOD0(Method0) \
  METHOD1(Method1, int) \
  METHOD2(Method2, int, String) \
  METHOD3(Method3, int, String, Value)
DEFINE_RPC_CLASS(TestRpcClass, TEST_RPC_STRUCT)

#define ANOTHER_TEST_RPC_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3, METHOD4) \
  METHOD0(Method0)
DEFINE_RPC_CLASS(AnotherTestRpcClass, ANOTHER_TEST_RPC_STRUCT)

class MockTestRpcClass : public TestRpcClassStub,
                         public DevToolsMockRpc {
 public:
  MockTestRpcClass() : TestRpcClassStub(NULL) {
    set_delegate(this);
  }
  ~MockTestRpcClass() {}
};

class MockAnotherTestRpcClass : public AnotherTestRpcClassStub,
                                public DevToolsMockRpc {
 public:
  MockAnotherTestRpcClass() : AnotherTestRpcClassStub(NULL) {
    set_delegate(this);
  }
  ~MockAnotherTestRpcClass() {}
};

class DevToolsRpcTests : public testing::Test {
 public:
  DevToolsRpcTests() {}

 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}
};

// Tests method call serialization.
TEST_F(DevToolsRpcTests, TestSerialize) {
  MockTestRpcClass mock;
  mock.Method0();
  EXPECT_EQ("[\"TestRpcClass\",\"Method0\"]", mock.get_log());
  mock.Reset();

  mock.Method1(10);
  EXPECT_EQ("[\"TestRpcClass\",\"Method1\",10]", mock.get_log());
  mock.Reset();

  mock.Method2(20, "foo");
  EXPECT_EQ("[\"TestRpcClass\",\"Method2\",20,\"foo\"]", mock.get_log());
  mock.Reset();

  StringValue value("bar");
  mock.Method3(30, "foo", value);
  EXPECT_EQ("[\"TestRpcClass\",\"Method3\",30,\"foo\",\"bar\"]",
      mock.get_log());
  mock.Reset();
}

// Tests method call dispatch.
TEST_F(DevToolsRpcTests, TestDispatch) {
  MockTestRpcClass local;
  MockTestRpcClass remote;

  // Call 1.
  local.Reset();
  local.Method0();
  remote.Reset();
  remote.Method0();
  remote.Replay();

  TestRpcClassDispatch::Dispatch(&remote, local.get_log());
  remote.Verify();

  // Call 2.
  local.Reset();
  local.Method1(10);
  remote.Reset();
  remote.Method1(10);
  remote.Replay();
  TestRpcClassDispatch::Dispatch(&remote, local.get_log());
  remote.Verify();

  // Call 3.
  local.Reset();
  remote.Reset();
  local.Method2(20, "foo");
  remote.Method2(20, "foo");

  remote.Replay();
  TestRpcClassDispatch::Dispatch(&remote, local.get_log());
  remote.Verify();

  // Call 4.
  local.Reset();
  remote.Reset();
  StringValue value("bar");
  local.Method3(30, "foo", value);
  remote.Method3(30, "foo", value);

  remote.Replay();
  TestRpcClassDispatch::Dispatch(&remote, local.get_log());
  remote.Verify();
}

}  // namespace
