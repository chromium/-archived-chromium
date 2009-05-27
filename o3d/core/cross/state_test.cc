/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file implements unit tests for class State.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/state.h"

namespace o3d {

class StateTest : public testing::Test {
 protected:

  StateTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Pack *pack_;
};

void StateTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void StateTest::TearDown() {
  pack_->Destroy();
}

TEST_F(StateTest, Basic) {
  State* state = pack()->Create<State>();
  // Check that state_set got created.
  ASSERT_TRUE(state != NULL);

  // Check that we can't create a param of the wrong type.
  EXPECT_TRUE(
      state->CreateParam<ParamMatrix4>(State::kFillModeParamName) == NULL);
}

TEST_F(StateTest, GetStateParam) {
  State* state = pack()->Create<State>();
  // Check that state_set got created.
  ASSERT_TRUE(state != NULL);

  // Check that we can get a state param by name.
  ParamBoolean* alpha_param = state->GetStateParam<ParamBoolean>(
      State::kAlphaBlendEnableParamName);
  EXPECT_TRUE(alpha_param != NULL);

  // Check that a name of a state that does not exist fails.
  EXPECT_TRUE(state->GetStateParam<ParamBoolean>("foobar") == NULL);

  // Check that a mis-matched state fails.
  EXPECT_TRUE(state->GetStateParam<ParamFloat>(
      State::kAlphaComparisonFunctionParamName) == NULL);

  // Check that if we create one by hand GetStateParam gets the same one.
  ParamBoolean* z_param = state->CreateParam<ParamBoolean>(
      State::kZEnableParamName);
  EXPECT_TRUE(z_param != NULL);
  EXPECT_EQ(z_param, state->GetStateParam<ParamBoolean>(
      State::kZEnableParamName));
}

}  // namespace o3d
