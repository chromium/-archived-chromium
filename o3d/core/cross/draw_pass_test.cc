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


// This file implements unit tests for class DrawPass.

#include "tests/common/win/testing_common.h"
#include "core/cross/draw_pass.h"
#include "core/cross/service_dependency.h"
#include "core/cross/object_manager.h"
#include "core/cross/transformation_context.h"
#include "core/cross/pack.h"

namespace o3d {

class DrawPassTest : public testing::Test {
 protected:

  DrawPassTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  TransformationContext* transformation_context_;
  Pack *pack_;
};

void DrawPassTest::SetUp() {
  transformation_context_ = new TransformationContext(g_service_locator);
  pack_ = object_manager_->CreatePack();
}

void DrawPassTest::TearDown() {
  object_manager_->DestroyPack(pack_);
  delete transformation_context_;
}

TEST_F(DrawPassTest, Basic) {
  DrawPass* draw_pass = pack()->Create<DrawPass>();
  // Check that draw_pass got created.
  EXPECT_TRUE(draw_pass != NULL);

  // Check that the default parameters got created.
  EXPECT_TRUE(draw_pass->GetParam<ParamDrawList>(
      DrawPass::kDrawListParamName) != NULL);
  EXPECT_TRUE(draw_pass->GetParam<ParamInteger>(
      DrawPass::kSortMethodParamName) != NULL);
}
}  // namespace o3d
