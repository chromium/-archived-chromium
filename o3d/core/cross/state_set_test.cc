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


// This file implements unit tests for class StateSet.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/state_set.h"

namespace o3d {

class StateSetTest : public testing::Test {
 protected:

  StateSetTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Pack *pack_;
};

void StateSetTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void StateSetTest::TearDown() {
  pack_->Destroy();
}

TEST_F(StateSetTest, Basic) {
  StateSet* state_set = pack()->Create<StateSet>();
  // Check that state_set got created.
  EXPECT_TRUE(state_set != NULL);

  // Check that the state param got created.
  EXPECT_TRUE(
      state_set->GetParam<ParamState>(StateSet::kStateParamName) != NULL);
}

}  // namespace o3d
